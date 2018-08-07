# <p >Create simulated real-time stream of Mini-SEED</p>

1. [Name](#)
1. [Synopsis](#synopsis)
1. [Description](#description)
1. [Options](#options)
1. [Input List File](#input-list-file)
1. [Match Or Reject List File](#match-or-reject-list-file)
1. [Error Handling And Return Codes](#error-handling-and-return-codes)
1. [Author](#author)

## <a id='synopsis'>Synopsis</a>

<pre >
mseedrtstream [options] file1 [file2 file3 ...]
</pre>

## <a id='description'>Description</a>

<p ><b>mseedrtstream</b> reads input files and creates simulated real-time feeds of Mini-SEED data. The most fundamental operation of this program is to read all specified input data and output the records in an order similar to what may be expected from a multiplexed real-time feed.  Data may be written to an output file or sent to a DataLink server (ringserver).</p>

<p >Multiple options are available to select a subset of data records from the specified input files.</p>

<p >Input files will be read in the order specified and processed all together as if all the data records were from the same file.  The program must read input data from files, input from pipes, etc. is not possible.  Output to a pipe is possible.</p>

<p >Files on the command line prefixed with a '@' character are input list files and are expected to contain a simple list of input files, see \fBINPUT LIST FILE\fR for more details.</p>

<p >When a input file is full SEED including both SEED headers and data records all of the headers will be skipped and completely unprocessed.</p>

## <a id='options'>Options</a>

<b>-V</b>

<p style="padding-left: 30px;">Print program version and exit.</p>

<b>-h</b>

<p style="padding-left: 30px;">Print program usage and exit.</p>

<b>-v</b>

<p style="padding-left: 30px;">Be more verbose.  This flag can be used multiple times ("-v -v" or "-vv") for more verbosity.</p>

<b>-sum</b>

<p style="padding-left: 30px;">Print a basic summary of input data after reading all the files.</p>

<b>-ts </b><i>time</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that start after or contain <i>time</i>.  The format of the <i>time</i> argument is: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' where valid delimiters are either commas (,), colons (:) or periods (.).</p>

<b>-te </b><i>time</i>

<p style="padding-left: 30px;">Limit processing to Mini-SEED records that end before or contain <i>time</i>.  The format of the <i>time</i> argument is: 'YYYY[,DDD,HH,MM,SS,FFFFFF]' where valid delimiters are either commas (,), colons (:) or periods (.).</p>

<b>-M </b><i>match</i>

<p style="padding-left: 30px;">Limit input to records that match this regular expression, the <i>match</i> is tested against the full source name: 'NET_STA_LOC_CHAN_QUAL'.  If the match expression begins with an '@' character it is assumed to indicate a file containing a list of expressions to match, see the \fBMATCH OR REJECT LIST FILE\fR section below.</p>

<b>-R </b><i>reject</i>

<p style="padding-left: 30px;">Limit input to records that do not match this regular expression, the <i>reject</i> is tested against the full source name: 'NET_STA_LOC_CHAN_QUAL'.  If the reject expression begins with an '@' character it is assumed to indicate a file containing a list of expressions to reject, see the \fBMATCH OR REJECT LIST FILE\fR section below.</p>

<b>-sd</b>

<p style="padding-left: 30px;">Stream delay.  Delay the output of miniSEED records in order to simulate a real-time stream.  By default records are output as fast as possible.</p>

<b>-df </b><i>factor</i>

<p style="padding-left: 30px;">Apply the <i>factor</i> to the delay in order to retard or accelerate the simulation of real-time streaming.  The default factor is 1.0 to mimic true time stepping.  Examples: a value of 2.0 will double the rate of simulated time and a value of 0.5 will slow the simulated rate of time to 1/2 true time.</p>

<b>-o </b><i>file</i>

<p style="padding-left: 30px;">Write simulated real-time data stream to output <i>file</i>.  If '-' is specified as the output file all output data will be written to standard out.  Any existing output file will be overwritten.</p>

<b>-dl </b><i>host:port</i>

<p style="padding-left: 30px;">Send simulated real-time data stream to DataLink server at <i>host</i> and <i>port</i>.</p>

## <a id='input-list-file'>Input List File</a>

<p >A list file can be used to specify input files, one file per line. The initial '@' character indicating a list file is not considered part of the file name.  As an example, if the following command line option was used:</p>

<pre >
<b>@files.list</b>
</pre>

<p >The 'files.list' file might look like this:</p>

<pre >
data/day1.mseed
data/day2.mseed
data/day3.mseed
</pre>

## <a id='match-or-reject-list-file'>Match Or Reject List File</a>

<p >A list file used with either the <b>-M</b> or <b>-R</b> contains a list of regular expressions (one on each line) that will be combined into a single compound expression.  The initial '@' character indicating a list file is not considered part of the file name.  As an example, if the following command line option was used:</p>

<pre >
<b>-M @match.list</b>
</pre>

<p >The 'match.list' file might look like this:</p>

<pre >
IU_ANMO_.*
IU_ADK_00_BHZ.*
II_BFO_00_BHZ_Q
</pre>

## <a id='error-handling-and-return-codes'>Error Handling And Return Codes</a>

<p >Any significant error message will be pre-pended with "ERROR" which can be parsed to determine run-time errors.  Additionally the program will return an exit code of 0 on successful operation and 1 when any errors were encountered.</p>

## <a id='author'>Author</a>

<pre >
Chad Trabant
IRIS Data Management Center
</pre>


(man page )
