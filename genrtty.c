// genrtty.c - make a RTTY stream

// Note: Compile me thus:  gcc -lm genrtty.c -o genrtty

// ===========
// includes
// ===========

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <tgmath.h>
#include <getopt.h>

// ================
// macros/defines
// ================

#define RATE   22050
#define MAXRATE   44100
#define BITS   16
#define CHANS  1
#define VOLPCT 20
// ^-- 90% max


// hacky
#define MAXSAMPLES (3200 * MAXRATE)

// uncomment only one of these
#define AUDIO_WAV
//#define AUDIO_AIFF

#define HEADER_BITS 1040
#define TRAILER_BITS 240

#define DEFAULT_BAUDRATE 300
// blehg
#define MARK_FREQ 1500
#define SPACE_FREQ 1200

#define DEFAULT_BITWIDTH 8
#define DEFAULT_PARITY 0
#define DEFAULT_STOPBITS 1

// =========
// globals
// =========

uint16_t g_audio[MAXSAMPLES];

uint32_t g_scale, g_samples;
double g_twopioverrate, g_uspersample;
double g_theta, g_fudge;

double g_bitlength;

FILE *g_imgfp;
FILE *g_outfp;
uint16_t g_rate;

uint32_t g_markfreq,g_spacefreq;
uint16_t g_baud;
uint8_t g_stopbits;
uint8_t g_parity;
uint8_t g_bitwidth;

// ========
// protos
// ========

uint8_t filetype (char *filename);
void playtone (uint16_t tonefreq, double tonedur);
void addheader (void);
//void addvistrailer (void);
uint16_t toneval (uint8_t colorval);
void buildaudio (void);

#ifdef AUDIO_AIFF
void writefile_aiff (void);
#endif
#ifdef AUDIO_WAV
void writefile_wav (void);
#endif


const char *parity_string[] = {
  "none",
  "odd",
  "even",
  "zero",
  "one",
  NULL
};


// ================
//   main
// ================

int
main (int argc, char *argv[])
{
  int errorexit = 0;

  int opt;

  g_rate = RATE;

  g_baud = DEFAULT_BAUDRATE;
  g_bitwidth = DEFAULT_BITWIDTH;
  g_parity = DEFAULT_PARITY;
  g_stopbits = DEFAULT_STOPBITS;
  g_markfreq = MARK_FREQ;
  g_spacefreq = SPACE_FREQ;

  while ((opt = getopt (argc, argv, "78w:s:p:b:r:o:z:")) != -1)
    {
      switch (opt)
	{
	case 'o':
	  g_markfreq = atoi (optarg);
	  break;

	case 'z':
	  g_spacefreq = atoi (optarg);
	  break;
	case 'p':
	  g_parity = atoi (optarg);
	  break;
	case 's':
	  g_stopbits = atoi (optarg);
	  break;
	case 'w':
	  g_bitwidth = atoi (optarg);
	  break;
	case '7':
	  g_bitwidth = 7;
	  break;
	case '8':
	  g_bitwidth = 8;
	  break;
	case 'b':
	  g_baud = atoi (optarg);
	  break;
	case 'r':
	  g_rate = atoi (optarg);
	  break;
	default:		/* '?' */
	  errorexit = 1;
	}
    }

  if (optind >= argc)
    {
      fprintf (stderr, "Expected argument after options\n");
      errorexit = 1;
    }
  if (g_rate > MAXRATE)
    {
      errorexit = 1;
    }

  if (abs(g_markfreq - g_spacefreq) < 20) {
   printf("[!] Get real with the mark/space shift please.\n");
  }
  if (g_markfreq < g_spacefreq) {
   printf("[!] Mark is usually higher than space freq.\n");
  }


  if (errorexit)
    {
      fprintf (stderr,
	       "Usage: %s whatever.txt [-p parity] [-s stopbits] [-b baud] [-r samprate]\n\t[-7 | -8 | -w bitwidth] [-o markfreq] [-z spacefreq]\n",
	       argv[0]);
      fprintf (stderr, " parity: 0=none 1=odd 2=even 3=zero 4=one\n");
      fprintf (stderr, "	default sample rate = %d\n", RATE);
      fprintf (stderr, "       	default baud rate   = %d\n", DEFAULT_BAUDRATE);
      fprintf (stderr, "       	default mark freq   = %d\n", MARK_FREQ);
      fprintf (stderr, "       	default space freq  = %d\n", SPACE_FREQ);
      fprintf (stderr, "	maximum samplerate  = %d\n", MAXRATE);
      return 1;
    }

  // locals
  uint32_t starttime = time (NULL);
  uint8_t ft;
  char inputfile[255], outputfile[255];

  // string hygeine
  memset (inputfile, 0, 255);
  memset (outputfile, 0, 255);

  // assign values to globals

  double temp1, temp2, temp3;
  temp1 = (double) (1 << (BITS - 1));
  temp2 = VOLPCT / 100.0;
  temp3 = temp1 * temp2;
  g_scale = (uint32_t) temp3;

  g_twopioverrate = 2.0 * M_PI / g_rate;
  g_uspersample = 1000000.0 / (double) g_rate;

  g_theta = 0.0;
  g_samples = 0.0;
  g_fudge = 0.0;
  g_bitlength = (1.00f / g_baud) * 1000000.0f;

/*
  printf ("Constants check:\n");
  printf ("      rate = %d\n", g_rate);
  printf ("      BITS = %d\n", BITS);
  printf ("    VOLPCT = %d\n", VOLPCT);
  printf ("     scale = %d\n", g_scale);
  printf ("   us/samp = %f\n", g_uspersample);
  printf ("   2p/rate = %f\n\n", g_twopioverrate);
*/
  printf ("      mark = %d hz\n", g_markfreq);
  printf ("     space = %d hz\n", g_spacefreq);


  printf (" Baud rate = %d bps\n"
	  " Bit width = %d bit\n"
	  "Parity bit = %s\n"
	  " Stop bits = %d bit\n"
	  "\n", g_baud, g_bitwidth, parity_string[g_parity], g_stopbits);
  printf ("    us/bit = %.3f\n", g_bitlength);

  // set filenames    
  strncpy (inputfile, argv[optind], strlen (argv[optind]));
  strncpy (outputfile, inputfile, strlen (inputfile));

#ifdef AUDIO_AIFF
  strcat (outputfile, ".aiff");
#endif
#ifdef AUDIO_WAV
  strcat (outputfile, ".wav");
#endif

  printf ("Input  file is [%s].\n", inputfile);
  printf ("Output file is [%s].\n", outputfile);

  // prep

  g_imgfp = fopen (inputfile, "r");
  g_outfp = fopen (outputfile, "w");

  if(g_imgfp == NULL) {
   printf("cannot open input file\n"); exit(1);
  }
  if(g_outfp == NULL) {
   printf("cannot open output file\n"); exit(1);
  }
//  printf ("FILE ptrs opened.\n");

  //addvisheader ();
  buildaudio ();
  //addvistrailer ();

#ifdef AUDIO_AIFF
  writefile_aiff ();
#endif
#ifdef AUDIO_WAV
  writefile_wav ();
#endif

  // cleanup

  fclose (g_imgfp);
  fclose (g_outfp);

  // brag

  uint32_t endtime = time (NULL);
  printf ("Created soundfile in %d seconds.\n", (endtime - starttime));

  return 0;
}


// =====================
//  subs 
// =====================    


// playtone -- Add waveform info to audio data. New waveform data is 
//             added in a phase-continuous manner according to the 
//             audio frequency and duration provided. Note that the
//             audio is still in a purely hypothetical state - the
//             format of the output file is not determined until 
//             the file is written, at the end of the process.
//             Also, yes, a nod to Tom Hanks.

void
playtone (uint16_t tonefreq, double tonedur)
{
  uint16_t tonesamples, voltage, i;
  double deltatheta;

  tonedur += g_fudge;
  tonesamples = (tonedur / g_uspersample) + 0.5;
  deltatheta = g_twopioverrate * tonefreq;

  for (i = 1; i <= tonesamples; i++)
    {
      g_samples++;

      if (tonefreq == 0)
	{
	  g_audio[g_samples] = 32768;
	}
      else
	{

#ifdef AUDIO_AIFF
	  voltage = 32768 + (int) (sin (g_theta) * g_scale);
#endif
#ifdef AUDIO_WAV
	  voltage = 0 + (int) (sin (g_theta) * g_scale);
#endif

	  g_audio[g_samples] = voltage;
	  g_theta += deltatheta;
	}
    }				// end for i        

  g_fudge = tonedur - (tonesamples * g_uspersample);
}				// end playtone    


unsigned char
getOddParity (unsigned char p)
{
  p = p ^ (p >> 4 | p << 4);
  p = p ^ (p >> 2);
  p = p ^ (p >> 1);
  return p & 1;
}

// buildaudio -- Build a RTTY stream

void
buildaudio ()
{
  unsigned long long i;
  int c, k;
  unsigned char j;
  printf ("Build RTTY stream:");

  playtone(g_markfreq, g_bitlength*HEADER_BITS);//header

  // loop on every character
  while ((c = fgetc (g_imgfp)) != EOF)
    {
      k = getOddParity ((char) c);
      // start bit
      playtone (g_spacefreq, g_bitlength);
      // data
      for (j = 0; j < g_bitwidth; j++)
	{
	  if (c & 1)
	    {
	      playtone (g_markfreq, g_bitlength);
	    }
	  else
	    {
	      playtone (g_spacefreq, g_bitlength);
	    }
	  c = c >> 1;
	}
      //parity
      switch (g_parity)
	{
	case 0:
	  break;
	case 2: //even
	  k = (~k) & 1;
	case 1: //odd
	  if (k & 1)
	    {
	      playtone (g_markfreq, g_bitlength);
	    }
	  else
	    {
	      playtone (g_spacefreq, g_bitlength);
	    };
	  break;
	case 3: //always 0
	      playtone (g_spacefreq, g_bitlength); break;
	case 4: // always 1
	      playtone (g_markfreq, g_bitlength); break;
	default:
	  break;
	}
      if((i%16)==0)
      printf (".");
      // stop bits
      for (j = 0; j < g_stopbits; j++)
	{
	  playtone (g_markfreq, g_bitlength);
	}
      i++;
    }
  playtone(g_markfreq, g_bitlength*TRAILER_BITS);//trailer

  printf ("Done.\n");

}
// end buildaudio


// writefile_aiff -- Save audio data to an AIFF file. Playback for
//                   AIFF format files is tricky. This worked on 
//                   ARM Linux:
//                     aplay -r 11025 -c 1 -f U16_BE file.aiff
//                   The WAV output is much easier and more portable, 
//                   but who knows - this code might be useful for 
//                   something. 

#ifdef AUDIO_AIFF
void
writefile_aiff ()
{
  uint32_t totalsize, audiosize, i;
  audiosize = 8 + (2 * g_samples);	// header + 2bytes/samp
  totalsize = 4 + 8 + 18 + 8 + audiosize;

  printf ("Writing audio data to file.\n");
  printf ("Got a total of [%d] samples (%f sec).\n", g_samples, (float)g_samples/g_rate);

  // "form" chunk
  fputs ("FORM", g_outfp);
  fputc ((totalsize & 0xff000000) >> 24, g_outfp);
  fputc ((totalsize & 0x00ff0000) >> 16, g_outfp);
  fputc ((totalsize & 0x0000ff00) >> 8, g_outfp);
  fputc ((totalsize & 0x000000ff), g_outfp);
  fputs ("AIFF", g_outfp);

  // "common" chunk
  fputs ("COMM", g_outfp);
  fputc (0, g_outfp);		// size
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (18, g_outfp);
  fputc (0, g_outfp);		// channels = 1
  fputc (1, g_outfp);
  fputc ((g_samples & 0xff000000) >> 24, g_outfp);	// size
  fputc ((g_samples & 0x00ff0000) >> 16, g_outfp);
  fputc ((g_samples & 0x0000ff00) >> 8, g_outfp);
  fputc ((g_samples & 0x000000ff), g_outfp);
  fputc (0, g_outfp);		// bits/sample
  fputc (16, g_outfp);
  fputc (0x40, g_outfp);	// 10 byte sample rate (??)
  fputc (0x0c, g_outfp);	// <--- 11025
  fputc (0xac, g_outfp);
  fputc (0x44, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);

  // audio data chunk
  fputs ("SSND", g_outfp);
  fputc ((audiosize & 0xff000000) >> 24, g_outfp);
  fputc ((audiosize & 0x00ff0000) >> 16, g_outfp);
  fputc ((audiosize & 0x0000ff00) >> 8, g_outfp);
  fputc ((audiosize & 0x000000ff), g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);

  // FINALLY, the audio data itself
  for (i = 0; i <= g_samples; i++)
    {
      fputc ((g_audio[i] & 0xff00) >> 8, g_outfp);
      fputc ((g_audio[i] & 0x00ff), g_outfp);
    }

  printf ("Done writing to audio file.\n");
}
#endif

// writefile_wav -- Write audio data to a WAV file. Once the file
//                  is written, any audio player in the world ought
//                  to be able to play the file without any funky
//                  command-line params.

#ifdef AUDIO_WAV
void
writefile_wav ()
{
  uint32_t totalsize, audiosize, byterate, blockalign;
  uint32_t i;

  audiosize = g_samples * CHANS * (BITS / 8);	// bytes of audio
  totalsize = 4 + (8 + 16) + (8 + audiosize);	// audio + some headers
  byterate = g_rate * CHANS * BITS / 8;	// audio bytes / sec
  blockalign = CHANS * BITS / 8;	// total bytes / sample

  printf ("Writing audio data to file.\n");
  printf ("Got a total of [%d] samples (%f sec).\n", g_samples, (float)g_samples/g_rate);

  // RIFF header 
  fputs ("RIFF", g_outfp);

  // total size, audio plus some headers (LE!!)
  fputc ((totalsize & 0x000000ff), g_outfp);
  fputc ((totalsize & 0x0000ff00) >> 8, g_outfp);
  fputc ((totalsize & 0x00ff0000) >> 16, g_outfp);
  fputc ((totalsize & 0xff000000) >> 24, g_outfp);
  fputs ("WAVE", g_outfp);

  // sub chunk 1 (format spec)

  fputs ("fmt ", g_outfp);	// with a space!

  fputc (16, g_outfp);		// size of chunk (LE!!)
  fputc (0, g_outfp);
  fputc (0, g_outfp);
  fputc (0, g_outfp);

  fputc (1, g_outfp);		// format = 1 (PCM) (LE)
  fputc (0, g_outfp);

  fputc (1, g_outfp);		// channels = 1 (LE)
  fputc (0, g_outfp);

  // samples / channel / sec (LE!!)
  fputc ((g_rate & 0x000000ff), g_outfp);
  fputc ((g_rate & 0x0000ff00) >> 8, g_outfp);
  fputc ((g_rate & 0x00ff0000) >> 16, g_outfp);
  fputc ((g_rate & 0xff000000) >> 24, g_outfp);

  // bytes total / sec (LE!!)
  fputc ((byterate & 0x000000ff), g_outfp);
  fputc ((byterate & 0x0000ff00) >> 8, g_outfp);
  fputc ((byterate & 0x00ff0000) >> 16, g_outfp);
  fputc ((byterate & 0xff000000) >> 24, g_outfp);

  // block alignment (LE!!)
  fputc ((blockalign & 0x00ff), g_outfp);
  fputc ((blockalign & 0xff00) >> 8, g_outfp);

  fputc ((BITS & 0x00ff), g_outfp);	// bits/sample (LE)
  fputc ((BITS & 0xff00) >> 8, g_outfp);

  // sub chunk 2
  // header
  fputs ("data", g_outfp);

  // audio bytes total (LE!!)
  fputc ((audiosize & 0x000000ff), g_outfp);
  fputc ((audiosize & 0x0000ff00) >> 8, g_outfp);
  fputc ((audiosize & 0x00ff0000) >> 16, g_outfp);
  fputc ((audiosize & 0xff000000) >> 24, g_outfp);

  // FINALLY, the audio data itself (LE!!)
  for (i = 0; i <= g_samples; i++)
    {
      fputc ((g_audio[i] & 0x00ff), g_outfp);
      fputc ((g_audio[i] & 0xff00) >> 8, g_outfp);
    }


  // no trailer    
  printf ("Done writing to audio file.\n");
}
#endif



// end
