/***************************************************************************
 * mseedrtstream.c - miniSEED data sorted to simulate a real-time stream.
 *
 * Opens one or more user specified files, applys filtering criteria
 * and outputs any matched data sorted into a time order that
 * simulates a real-time stream.
 *
 * In general critical error messages are prefixed with "ERROR:" and
 * the return code will be 1.  On successfull operation the return
 * code will be 0.
 *
 * Written by Chad Trabant, IRIS Data Management Center.
 ***************************************************************************/

/* ToDo? Restamp record start times to simulate current data flow */

#include <errno.h>
#include <math.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <libdali.h>
#include <libmseed.h>

#define VERSION "0.3"
#define PACKAGE "mseedrtstream"

/* Input/output file information containers */
typedef struct Filelink_s
{
  char *infilename; /* Input file name */
  FILE *infp;       /* Input file descriptor */
  struct Filelink_s *next;
} Filelink;

/* miniSEED record information structures */
typedef struct Record_s
{
  struct Filelink_s *flp;
  off_t offset;
  int reclen;
  hptime_t starttime;
  hptime_t endtime;
  struct Record_s *prev;
  struct Record_s *next;
} Record;

/* Record map, holds Record structures for a given MSTrace */
typedef struct RecordMap_s
{
  long long int recordcnt;
  struct Record_s *first;
  struct Record_s *last;
} RecordMap;

static int readfiles (RecordMap *recmap);
static int writerecords (RecordMap *recmap);
static int sendrecord (char *recbuf, Record *rec);

static int sortrecmap (RecordMap *recmap);
static int recordcmp (Record *rec1, Record *rec2);

static int processparam (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt);
static hptime_t gethptime (void);
static int setofilelimit (int limit);
static int addfile (char *filename);
static int addlistfile (char *filename);
static int readregexfile (char *regexfile, char **pppattern);
static void usage (void);

static flag verbose  = 0;
static flag basicsum = 0;  /* Controls printing of basic summary */
static int reclen    = -1; /* Input data record length, autodetected in most cases */

static hptime_t starttime = HPTERROR; /* Limit to records containing or after starttime */
static hptime_t endtime   = HPTERROR; /* Limit to records containing or before endtime */

static regex_t *match  = 0; /* Compiled match regex */
static regex_t *reject = 0; /* Compiled reject regex */

static flag streamdelay   = 0;   /* Delay output to simulate real time stream */
static double delayfactor = 1.0; /* Delay factor, 1.0 is actual time stepping */

static char *outputfile = 0; /* Single output file */

static char recordbuf[16384]; /* Global record buffer */

static Filelink *filelist     = 0; /* List of input files */
static Filelink *filelisttail = 0; /* Tail of list of input files */

static DLCP *dlconn = 0;

int
main (int argc, char **argv)
{
  RecordMap recmap;

  /* Set default error message prefix */
  ms_loginit (NULL, NULL, NULL, "ERROR: ");

  /* Process input parameters */
  if (processparam (argc, argv) < 0)
    return 1;

  /* Connect to DataLink server */
  if (dlconn)
  {
    if (dl_connect (dlconn) < 0)
    {
      ms_log (2, "Error connecting to DataLink server\n");
      return -1;
    }
  }

  recmap.recordcnt = 0;
  recmap.first     = NULL;
  recmap.last      = NULL;

  if (verbose > 1)
    ms_log (1, "Reading input files\n");

  /* Read and process all files specified on the command line */
  if (readfiles (&recmap))
    return 1;

  if (verbose > 1)
    ms_log (1, "Sorting record list\n");

  /* Sort the record map into time order */
  if (sortrecmap (&recmap))
  {
    ms_log (2, "Error sorting record list\n");
    return 1;
  }

  /* Write records */
  if (writerecords (&recmap))
    return 1;

  /* Shut down the connection to DataLink server */
  if (dlconn && dlconn->link != -1)
    dl_disconnect (dlconn);

  return 0;
} /* End of main() */

/***************************************************************************
 * readfiles:
 *
 * Read input files specified as a Filelink list and populate an
 * RecordMap (list of records).
 *
 * Returns 0 on success and -1 otherwise.
 ***************************************************************************/
static int
readfiles (RecordMap *recmap)
{
  MSFileParam *msfp = NULL;
  Filelink *flp;
  MSRecord *msr = 0;

  int totalrecs  = 0;
  int totalsamps = 0;
  int totalfiles = 0;

  Record *rec = 0;

  off_t fpos = 0;
  hptime_t recstarttime;
  hptime_t recendtime;

  char srcname[50];
  char stime[30];

  int retcode;

  /* Read all input files and populate record list */

  flp = filelist;

  while (flp != 0)
  {
    /* Loop over the input file */
    while ((retcode = ms_readmsr_main (&msfp, &msr, flp->infilename, reclen, &fpos, NULL, 1, 0, NULL, verbose - 2)) == MS_NOERROR)
    {
      recstarttime = msr->starttime;
      recendtime   = msr_endtime (msr);

      /* Generate the srcname with the quality code */
      msr_srcname (msr, srcname, 1);

      /* Check if record matches start time criteria: starts after or contains starttime */
      if ((starttime != HPTERROR) && (recstarttime < starttime && !(recstarttime <= starttime && recendtime >= starttime)))
      {
        if (verbose >= 3)
        {
          ms_hptime2seedtimestr (recstarttime, stime, 1);
          ms_log (1, "Skipping (starttime) %s, %s\n", srcname, stime);
        }
        continue;
      }

      /* Check if record matches end time criteria: ends after or contains endtime */
      if ((endtime != HPTERROR) && (recendtime > endtime && !(recstarttime <= endtime && recendtime >= endtime)))
      {
        if (verbose >= 3)
        {
          ms_hptime2seedtimestr (recstarttime, stime, 1);
          ms_log (1, "Skipping (endtime) %s, %s\n", srcname, stime);
        }
        continue;
      }

      /* Check if record is matched by the match regex */
      if (match)
      {
        if (regexec (match, srcname, 0, 0, 0) != 0)
        {
          if (verbose >= 3)
          {
            ms_hptime2seedtimestr (recstarttime, stime, 1);
            ms_log (1, "Skipping (match) %s, %s\n", srcname, stime);
          }
          continue;
        }
      }

      /* Check if record is rejected by the reject regex */
      if (reject)
      {
        if (regexec (reject, srcname, 0, 0, 0) == 0)
        {
          if (verbose >= 3)
          {
            ms_hptime2seedtimestr (recstarttime, stime, 1);
            ms_log (1, "Skipping (reject) %s, %s\n", srcname, stime);
          }
          continue;
        }
      }

      if (verbose > 2)
        msr_print (msr, verbose - 3);

      /* Create and populate new Record structure */
      if (!(rec = (Record *)malloc (sizeof (Record))))
      {
        ms_log (2, "Cannot allocate memory for Record entry\n");
        return -1;
      }

      rec->flp       = flp;
      rec->offset    = fpos;
      rec->reclen    = msr->reclen;
      rec->starttime = recstarttime;
      rec->endtime   = recendtime;
      rec->prev      = 0;
      rec->next      = 0;

      /* Add the new Record to end of the RecordMap */
      if (!recmap->first)
        recmap->first = rec;

      if (recmap->last)
        recmap->last->next = rec;

      rec->prev    = recmap->last;
      recmap->last = rec;

      recmap->recordcnt++;

      totalrecs++;
      totalsamps += msr->samplecnt;
    } /* End of looping through records in file */

    /* Critical error if file was not read properly */
    if (retcode != MS_ENDOFFILE)
    {
      ms_log (2, "Cannot read %s: %s\n", flp->infilename, ms_errorstr (retcode));
      ms_readmsr_main (&msfp, &msr, NULL, 0, NULL, NULL, 0, 0, NULL, 0);
      return -1;
    }

    /* Make sure everything is cleaned up */
    ms_readmsr_main (&msfp, &msr, NULL, 0, NULL, NULL, 0, 0, NULL, 0);

    totalfiles++;
    flp = flp->next;
  } /* End of looping over file list */

  /* Increase open file limit if necessary, in general we need the
   * filecount and some wiggle room. */
  setofilelimit (totalfiles + 20);

  if (basicsum)
    ms_log (0, "Files: %d, Records: %d, Samples: %d\n", totalfiles, totalrecs, totalsamps);

  return 0;
} /* End of readfiles() */

/***************************************************************************
 * writerecords():
 *
 * Write all records in the RecordMap to output.
 *
 * Returns 0 on success and 1 on error.
 ***************************************************************************/
static int
writerecords (RecordMap *recmap)
{
  static uint64_t totalrecsout  = 0;
  static uint64_t totalbytesout = 0;
  hptime_t now;
  hptime_t offset = HPTERROR;
  Filelink *flp;
  Record *rec;
  char errflag = 0;

  FILE *ofp = 0;

  if (!recmap)
    return 1;

  /* Open the output file if specified */
  if (outputfile)
  {
    if (verbose)
      ms_log (1, "Writing output data to %s\n", outputfile);

    if (strcmp (outputfile, "-") == 0)
    {
      ofp = stdout;
    }
    else if ((ofp = fopen (outputfile, "wb")) == NULL)
    {
      ms_log (2, "Cannot open output file: %s (%s)\n",
              outputfile, strerror (errno));
      return 1;
    }
  }

  if (dlconn)
  {
    if (verbose)
      ms_log (1, "Sending output data to %s\n", dlconn->addr);
  }

  /* Loop through record list and send/write records */
  rec = recmap->first;
  while (rec && errflag != 1)
  {
    /* Reset error flag for continuation errors */
    if (errflag == 2)
      errflag = 0;

    /* Make sure the record buffer is large enough */
    if (rec->reclen > sizeof (recordbuf))
    {
      ms_log (2, "Record length (%d bytes) larger than buffer (%llu bytes)\n",
              rec->reclen, (long long unsigned int)sizeof (recordbuf));
      errflag = 1;
      break;
    }

    /* Open file for reading if not already done */
    if (!rec->flp->infp)
      if (!(rec->flp->infp = fopen (rec->flp->infilename, "rb")))
      {
        ms_log (2, "Cannot open '%s' for reading: %s\n",
                rec->flp->infilename, strerror (errno));
        errflag = 1;
        break;
      }

    /* Seek to record offset */
    if (lmp_fseeko (rec->flp->infp, rec->offset, SEEK_SET) == -1)
    {
      ms_log (2, "Cannot seek in '%s': %s\n",
              rec->flp->infilename, strerror (errno));
      errflag = 1;
      break;
    }

    /* Read record into buffer */
    if (fread (recordbuf, rec->reclen, 1, rec->flp->infp) != 1)
    {
      ms_log (2, "Cannot read %d bytes at offset %llu from '%s'\n",
              rec->reclen, (long long unsigned)rec->offset,
              rec->flp->infilename);
      errflag = 1;
      break;
    }

    if (verbose > 1)
    {
      char srcname[50];
      char timestr[50];
      ms_recsrcname (recordbuf, srcname, 0);
      ms_hptime2isotimestr (rec->starttime, timestr, 1);
      ms_log (1, "Writing %s %s\n", srcname, timestr);
    }

    if (streamdelay)
    {
      hptime_t snooze;
      now = gethptime ();

      /* Calculate offset between initial packet and current time */
      if (offset == HPTERROR)
        offset = now - rec->endtime;

      snooze = offset - (now - rec->endtime);

      if (snooze > 0)
      {
        if (verbose > 1)
          ms_log (1, "Sleeping %.2f seconds to simulate streaming\n",
                  (double)MS_HPTIME2EPOCH (snooze / delayfactor));

        dlp_usleep ((unsigned long int)(snooze / delayfactor + 0.5));
      }
    }

    /* Write to a single output file if specified */
    if (ofp)
    {
      if (fwrite (recordbuf, rec->reclen, 1, ofp) != 1)
      {
        ms_log (2, "Cannot write to '%s'\n", outputfile);
        errflag = 1;
        break;
      }
    }

    /* Send to DataLink server if specified */
    if (dlconn)
    {
      while (sendrecord (recordbuf, rec))
      {
        if (verbose)
          ms_log (1, "Re-connecting to DataLink server\n");

        /* Re-connect to DataLink server and sleep if error connecting */
        if (dlconn->link != -1)
          dl_disconnect (dlconn);

        if (dl_connect (dlconn) < 0)
        {
          ms_log (2, "Error re-connecting to DataLink server, sleeping 10 seconds\n");
          sleep (10);
        }
      }
    }

    totalrecsout++;
    totalbytesout += rec->reclen;

    rec = rec->next;
  } /* Done looping through records */

  /* Close all open input & output files */
  flp = filelist;
  while (flp)
  {
    if (flp->infp)
    {
      fclose (flp->infp);
      flp->infp = 0;
    }

    flp = flp->next;
  }

  /* Close output file if used */
  if (ofp)
  {
    fclose (ofp);
    ofp = 0;
  }

  if (verbose)
  {
    ms_log (1, "Wrote %llu bytes of %llu records to output\n",
            totalbytesout, totalrecsout);
  }

  return (errflag) ? 1 : 0;
} /* End of writerecords() */

/***************************************************************************
 * sendrecord:
 *
 * Send the specified record to the DataLink server.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
sendrecord (char *recbuf, Record *rec)
{
  char streamid[100];

  if (!recbuf || !rec)
    return -1;

  /* Generate stream ID for this record: NET_STA_LOC_CHAN/MSEED */
  ms_recsrcname (recbuf, streamid, 0);
  strcat (streamid, "/MSEED");

  /* Send record to server */
  if (dl_write (dlconn, recbuf, rec->reclen, streamid,
                rec->starttime, rec->endtime, 0) < 0)
  {
    return -1;
  }

  return 0;
} /* End of sendrecord() */

/***************************************************************************
 * sortrecmap():
 *
 * Sort a RecordMap so that records are in time order using a
 * mergesort algorithm.
 *
 * The mergesort implementation was inspired by the listsort function
 * published and copyright 2001 by Simon Tatham.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
static int
sortrecmap (RecordMap *recmap)
{
  Record *p, *q, *e, *top, *tail;
  int nmerges;
  int insize, psize, qsize, i;

  if (!recmap)
    return -1;

  if (!recmap->first || !recmap->last) /* Done if empty */
    return 0;

  top    = recmap->first;
  insize = 1;

  for (;;)
  {
    p    = top;
    top  = NULL;
    tail = NULL;

    nmerges = 0; /* count number of merges we do in this pass */

    while (p)
    {
      nmerges++; /* there exists a merge to be done */

      /* step `insize' places along from p */
      q     = p;
      psize = 0;
      for (i = 0; i < insize; i++)
      {
        psize++;
        q = q->next;
        if (!q)
          break;
      }

      /* if q hasn't fallen off end, we have two lists to merge */
      qsize = insize;

      /* now we have two lists; merge them */
      while (psize > 0 || (qsize > 0 && q))
      {
        /* decide whether next element of merge comes from p or q */
        if (psize == 0)
        { /* p is empty; e must come from q. */
          e = q;
          q = q->next;
          qsize--;
        }
        else if (qsize == 0 || !q)
        { /* q is empty; e must come from p. */
          e = p;
          p = p->next;
          psize--;
        }
        else if (recordcmp (p, q) <= 0)
        { /* First element of p is lower (or same), e must come from p. */
          e = p;
          p = p->next;
          psize--;
        }
        else
        { /* First element of q is lower; e must come from q. */
          e = q;
          q = q->next;
          qsize--;
        }

        /* add the next element to the merged list */
        if (tail)
          tail->next = e;
        else
          top = e;

        tail = e;
      }

      /* now p has stepped `insize' places along, and q has too */
      p = q;
    }

    tail->next = NULL;

    /* If we have done only one merge, we're finished. */
    if (nmerges <= 1) /* allow for nmerges==0, the empty list case */
    {
      recmap->first = top;
      recmap->last  = tail;

      return 0;
    }

    /* Otherwise repeat, merging lists twice the size */
    insize *= 2;
  }
} /* End of sortrecmap() */

/***************************************************************************
 * recordcmp():
 *
 * Compare the start times of each Record for the purposes of sorting
 * a RecordMap.
 *
 * Return 1 if rec1 is "greater" than rec2, otherwise return 0.
 ***************************************************************************/
static int
recordcmp (Record *rec1, Record *rec2)
{
  if (!rec1 || !rec2)
    return -1;

  if (rec1->endtime > rec2->endtime)
  {
    return 1;
  }

  return 0;
} /* End of recordcmp() */

/***************************************************************************
 * processparam():
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
processparam (int argcount, char **argvec)
{
  int optind;
  char *selectfile    = 0;
  char *matchpattern  = 0;
  char *rejectpattern = 0;
  char *dladdress     = 0;
  char *tptr;

  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
  {
    if (strcmp (argvec[optind], "-V") == 0)
    {
      ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
      exit (0);
    }
    else if (strcmp (argvec[optind], "-h") == 0)
    {
      usage ();
      exit (0);
    }
    else if (strncmp (argvec[optind], "-v", 2) == 0)
    {
      verbose += strspn (&argvec[optind][1], "v");
    }
    else if (strcmp (argvec[optind], "-sum") == 0)
    {
      basicsum = 1;
    }
    else if (strcmp (argvec[optind], "-s") == 0)
    {
      selectfile = getoptval (argcount, argvec, optind++);
    }
    else if (strcmp (argvec[optind], "-ts") == 0)
    {
      starttime = ms_seedtimestr2hptime (getoptval (argcount, argvec, optind++));
      if (starttime == HPTERROR)
        return -1;
    }
    else if (strcmp (argvec[optind], "-te") == 0)
    {
      endtime = ms_seedtimestr2hptime (getoptval (argcount, argvec, optind++));
      if (endtime == HPTERROR)
        return -1;
    }
    else if (strcmp (argvec[optind], "-M") == 0)
    {
      matchpattern = strdup (getoptval (argcount, argvec, optind++));
    }
    else if (strcmp (argvec[optind], "-R") == 0)
    {
      rejectpattern = strdup (getoptval (argcount, argvec, optind++));
    }
    else if (strcmp (argvec[optind], "-sd") == 0)
    {
      streamdelay = 1;
    }
    else if (strcmp (argvec[optind], "-df") == 0)
    {
      streamdelay = 1;
      delayfactor = strtod (getoptval (argcount, argvec, optind++), NULL);
    }
    else if (strcmp (argvec[optind], "-o") == 0)
    {
      outputfile = getoptval (argcount, argvec, optind++);
    }
    else if (strcmp (argvec[optind], "-dl") == 0)
    {
      dladdress = getoptval (argcount, argvec, optind++);
    }
    else if (strncmp (argvec[optind], "-", 1) == 0 &&
             strlen (argvec[optind]) > 1)
    {
      ms_log (2, "Unknown option: %s\n", argvec[optind]);
      exit (1);
    }
    else
    {
      tptr = argvec[optind];

      /* Check for an input file list */
      if (tptr[0] == '@')
      {
        if (addlistfile (tptr + 1) < 0)
        {
          ms_log (2, "Error adding list file %s", tptr + 1);
          exit (1);
        }
      }
      /* Otherwise this is an input file */
      else
      {
        /* Add file to global file list */
        if (addfile (tptr))
        {
          ms_log (2, "Error adding file to input list %s", tptr);
          exit (1);
        }
      }
    }
  }

  /* Make sure input file(s) were specified */
  if (!filelist)
  {
    ms_log (2, "No input files were specified\n\n");
    ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
    ms_log (1, "Try %s -h for usage\n", PACKAGE);
    exit (0);
  }

  /* Make sure output file or server was specified */
  if (!outputfile && !dladdress)
  {
    ms_log (2, "No output file or server was specified\n\n");
    ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
    ms_log (1, "Try %s -h for usage\n", PACKAGE);
    exit (0);
  }

  /* Allocate and initialize DataLink connection description */
  if (dladdress && !(dlconn = dl_newdlcp (dladdress, argvec[0])))
  {
    ms_log (2, "Cannot allocation DataLink descriptor\n");
    exit (1);
  }

  /* Expand match pattern from a file if prefixed by '@' */
  if (matchpattern)
  {
    if (*matchpattern == '@')
    {
      tptr = strdup (matchpattern + 1); /* Skip the @ sign */
      free (matchpattern);
      matchpattern = 0;

      if (readregexfile (tptr, &matchpattern) <= 0)
      {
        ms_log (2, "Cannot read match pattern regex file\n");
        exit (1);
      }

      free (tptr);
    }
  }

  /* Expand reject pattern from a file if prefixed by '@' */
  if (rejectpattern)
  {
    if (*rejectpattern == '@')
    {
      tptr = strdup (rejectpattern + 1); /* Skip the @ sign */
      free (rejectpattern);
      rejectpattern = 0;

      if (readregexfile (tptr, &rejectpattern) <= 0)
      {
        ms_log (2, "Cannot read reject pattern regex file\n");
        exit (1);
      }

      free (tptr);
    }
  }

  /* Compile match and reject patterns */
  if (matchpattern)
  {
    if (!(match = (regex_t *)malloc (sizeof (regex_t))))
    {
      ms_log (2, "Cannot allocate memory for match expression\n");
      exit (1);
    }

    if (regcomp (match, matchpattern, REG_EXTENDED) != 0)
    {
      ms_log (2, "Cannot compile match regex: '%s'\n", matchpattern);
    }

    free (matchpattern);
  }

  if (rejectpattern)
  {
    if (!(reject = (regex_t *)malloc (sizeof (regex_t))))
    {
      ms_log (2, "Cannot allocate memory for reject expression\n");
      exit (1);
    }

    if (regcomp (reject, rejectpattern, REG_EXTENDED) != 0)
    {
      ms_log (2, "Cannot compile reject regex: '%s'\n", rejectpattern);
    }

    free (rejectpattern);
  }

  /* Report the program version */
  if (verbose)
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);

  return 0;
} /* End of processparam() */

/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt)
{
  if (argvec == NULL || argvec[argopt] == NULL)
  {
    ms_log (2, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }

  /* Special case of '-o -' usage */
  if ((argopt + 1) < argcount && strcmp (argvec[argopt], "-o") == 0)
    if (strcmp (argvec[argopt + 1], "-") == 0)
      return argvec[argopt + 1];

  /* Special case of '-s -' usage */
  if ((argopt + 1) < argcount && strcmp (argvec[argopt], "-s") == 0)
    if (strcmp (argvec[argopt + 1], "-") == 0)
      return argvec[argopt + 1];

  if ((argopt + 1) < argcount && *argvec[argopt + 1] != '-')
    return argvec[argopt + 1];

  ms_log (2, "Option %s requires a value, try -h for usage\n", argvec[argopt]);
  exit (1);
  return 0;
} /* End of getoptval() */

/***********************************************************************/ /**
 * gethptime:
 *
 * Determine the current time from the system as a hptime_t value.
 *
 * Returns current time as a hptime_t value.
 ***************************************************************************/
static hptime_t
gethptime (void)
{
  hptime_t hptime;
  struct timeval tv;

  if (gettimeofday (&tv, (struct timezone *)0))
  {
    return HPTERROR;
  }

  hptime = ((int64_t)tv.tv_sec * DLTMODULUS) +
           ((int64_t)tv.tv_usec * (DLTMODULUS / 1000000));

  return hptime;
} /* End of gethptime() */

/***************************************************************************
 * setofilelimit:
 *
 * Check the current open file limit and if it is not >= 'limit' try
 * to increase it to 'limit'.
 *
 * Returns the open file limit on success and -1 on error.
 ***************************************************************************/
static int
setofilelimit (int limit)
{
  struct rlimit rlim;
  rlim_t oldlimit;

  /* Get the current soft open file limit */
  if (getrlimit (RLIMIT_NOFILE, &rlim) == -1)
  {
    ms_log (2, "getrlimit() failed to get open file limit\n");
    return -1;
  }

  if (rlim.rlim_cur < limit)
  {
    oldlimit      = rlim.rlim_cur;
    rlim.rlim_cur = (rlim_t)limit;

    if (verbose > 1)
      ms_log (1, "Setting open file limit to %d\n",
              (int)rlim.rlim_cur);

    if (setrlimit (RLIMIT_NOFILE, &rlim) == -1)
    {
      ms_log (2, "setrlimit failed to raise open file limit from %d to %d (max: %d)\n",
              (int)oldlimit, (int)limit, rlim.rlim_max);
      return -1;
    }
  }

  return (int)rlim.rlim_cur;
} /* End of setofilelimit() */

/***************************************************************************
 * addfile:
 *
 * Add file to end of the specified file list.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
static int
addfile (char *filename)
{
  Filelink *newlp;

  if (!filename)
  {
    ms_log (2, "addfile(): No file name specified\n");
    return -1;
  }

  if (!(newlp = (Filelink *)calloc (1, sizeof (Filelink))))
  {
    ms_log (2, "addfile(): Cannot allocate memory\n");
    return -1;
  }

  if (!(newlp->infilename = strdup (filename)))
  {
    ms_log (2, "addfile(): Cannot duplicate string\n");
    return -1;
  }

  /* Add new file to the end of the list */
  if (filelisttail == 0)
  {
    filelist     = newlp;
    filelisttail = newlp;
  }
  else
  {
    filelisttail->next = newlp;
    filelisttail       = newlp;
  }

  return 0;
} /* End of addfile() */

/***************************************************************************
 * addlistfile:
 *
 * Add files listed in the specified file to the global input file list.
 *
 * Returns count of files added on success and -1 on error.
 ***************************************************************************/
static int
addlistfile (char *filename)
{
  FILE *fp;
  char filelistent[1024];
  int filecount = 0;

  if (verbose >= 1)
    ms_log (1, "Reading list file '%s'\n", filename);

  if (!(fp = fopen (filename, "rb")))
  {
    ms_log (2, "Cannot open list file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  while (fgets (filelistent, sizeof (filelistent), fp))
  {
    char *cp;

    /* End string at first newline character */
    if ((cp = strchr (filelistent, '\n')))
      *cp = '\0';

    /* Skip empty lines */
    if (!strlen (filelistent))
      continue;

    /* Skip comment lines */
    if (*filelistent == '#')
      continue;

    if (verbose > 1)
      ms_log (1, "Adding '%s' from list file\n", filelistent);

    if (addfile (filelistent))
      return -1;

    filecount++;
  }

  fclose (fp);

  return filecount;
} /* End of addlistfile() */

/***************************************************************************
 * readregexfile:
 *
 * Read a list of regular expressions from a file and combine them
 * into a single, compound expression which is returned in *pppattern.
 * The return buffer is reallocated as need to hold the growing
 * pattern.  When called *pppattern should not point to any associated
 * memory.
 *
 * Returns the number of regexes parsed from the file or -1 on error.
 ***************************************************************************/
static int
readregexfile (char *regexfile, char **pppattern)
{
  FILE *fp;
  char line[1024];
  char linepattern[1024];
  int regexcnt = 0;
  int lengthbase;
  int lengthadd;

  if (!regexfile)
  {
    ms_log (2, "readregexfile: regex file not supplied\n");
    return -1;
  }

  if (!pppattern)
  {
    ms_log (2, "readregexfile: pattern string buffer not supplied\n");
    return -1;
  }

  /* Open the regex list file */
  if ((fp = fopen (regexfile, "rb")) == NULL)
  {
    ms_log (2, "Cannot open regex list file %s: %s\n",
            regexfile, strerror (errno));
    return -1;
  }

  if (verbose)
    ms_log (1, "Reading regex list from %s\n", regexfile);

  *pppattern = NULL;

  while ((fgets (line, sizeof (line), fp)) != NULL)
  {
    /* Trim spaces and skip if empty lines */
    if (sscanf (line, " %s ", linepattern) != 1)
      continue;

    /* Skip comment lines */
    if (*linepattern == '#')
      continue;

    regexcnt++;

    /* Add regex to compound regex */
    if (*pppattern)
    {
      lengthbase = strlen (*pppattern);
      lengthadd  = strlen (linepattern) + 4; /* Length of addition plus 4 characters: |()\0 */

      *pppattern = realloc (*pppattern, lengthbase + lengthadd);

      if (*pppattern)
      {
        snprintf ((*pppattern) + lengthbase, lengthadd, "|(%s)", linepattern);
      }
      else
      {
        ms_log (2, "Cannot allocate memory for regex string\n");
        return -1;
      }
    }
    else
    {
      lengthadd = strlen (linepattern) + 3; /* Length of addition plus 3 characters: ()\0 */

      *pppattern = malloc (lengthadd);

      if (*pppattern)
      {
        snprintf (*pppattern, lengthadd, "(%s)", linepattern);
      }
      else
      {
        ms_log (2, "Cannot allocate memory for regex string\n");
        return -1;
      }
    }
  }

  fclose (fp);

  return regexcnt;
} /* End of readregexfile() */

/***************************************************************************
 * usage():
 * Print the usage message.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s - Create simulated real-time stream of miniSEED: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] file1 [file2] [file3] ...\n\n", PACKAGE);
  fprintf (stderr,
           " ## Options ##\n"
           " -V           Report program version\n"
           " -h           Show this usage message\n"
           " -v           Be more verbose, multiple flags can be used\n"
           " -sum         Print a basic summary after reading all input files\n"
           "\n"
           " ## Data selection options ##\n"
           " -ts time     Limit to records that contain or start after time\n"
           " -te time     Limit to records that contain or end before time\n"
           "                time format: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' delimiters: [,:.]\n"
           " -M match     Limit to records matching the specified regular expression\n"
           " -R reject    Limit to records not matching the specfied regular expression\n"
           "                Regular expressions are applied to: 'NET_STA_LOC_CHAN_QUAL'\n"
           "\n"
           " -sd          Delay output of data to simulate real time flow\n"
           " -df factor   Delay factor, to retard or accelerate simulated time, default 1\n"
           "\n"
           " ## Output and input options ##\n"
           " -o file      Specify an output file\n"
           " -dl server   Specify a DataLink server destination in host:port format\n"
           "\n"
           " file#        Files(s) of miniSEED records\n"
           "\n");
} /* End of usage() */
