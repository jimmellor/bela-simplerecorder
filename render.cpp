#include <Bela.h>
#include <sndfile.h>
#include <string>

#define REC_BUFFER_LEN 1024

using namespace std;

// Filename to record to. This must be created before this example is run.
string gRecordTrackName = "record.wav";

bool gBufferWriting = false; // know when the buffer has written to disk
bool gActiveBuffer = false;

float gRecBuffer[2][REC_BUFFER_LEN]; // double buffer, one for recording, the other for writing to disk

int gReadPtr = REC_BUFFER_LEN;

AuxiliaryTask gFillBufferTask;

SNDFILE *recsndfile ; // File we'll record to
SF_INFO recsfinfo ; // Store info about the sound file

int writeAudio(SNDFILE *sndfile, SF_INFO sfinfo, float *buffer, int samples){
	sf_count_t wroteSamples = sf_write_float(sndfile, buffer, samples);
	int err = sf_error(sndfile);
	if (err) {
		rt_printf("write generated error %i : \n", err);
	}
	return wroteSamples;
}

void fillBuffer(void*) {
	gBufferWriting = true;
    int writeFrames = REC_BUFFER_LEN;
   	writeAudio(recsndfile, recsfinfo, gRecBuffer[!gActiveBuffer], writeFrames);
}

bool setup(BelaContext *context, void *userData)
{
	// Initialise auxiliary tasks
	if((gFillBufferTask = Bela_createAuxiliaryTask(&fillBuffer, 90, "fill-buffer")) == 0)
		return false;
		
	// Open the record sound file
	recsfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT; // Specify the write format, see sndfile.h for more info.
	recsfinfo.samplerate = context->audioSampleRate ; // Use the Bela context to set sample rate
	recsfinfo.channels = context->audioOutChannels ; // Use the Bela context to set number of channels
	if (!(recsndfile = sf_open (gRecordTrackName.c_str(), SFM_WRITE, &recsfinfo))) {
		rt_printf("Couldn't open file %s : %s\n",gRecordTrackName.c_str(),sf_strerror(recsndfile));
	}
	
	// Locate the start of the record file
	sf_seek(recsndfile, 0, SEEK_SET);
	
	// We should see a file with the correct number of channels and zero frames
	rt_printf("Record file contains %i channel(s)\n", recsfinfo.channels);
	
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		//record audio interleaved in gRecBuffer
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		// when gRecBuffer is full, switch to other buffer, write audio to disk using the callback
    		if(++gReadPtr >= REC_BUFFER_LEN) {
	            if(!gBufferWriting)
	                rt_printf("Couldn't write buffer in time -- try increasing buffer size");
	            Bela_scheduleAuxiliaryTask(gFillBufferTask);
	            // switch buffer
	            gActiveBuffer = !gActiveBuffer;
	            // clear the buffer writing flag
	            gBufferWriting = false;
	            gReadPtr = 0;
        	}
        	// store the sample from the audioRead buffer in the active buffer
    		gRecBuffer[gActiveBuffer][gReadPtr] = audioRead(context, n, channel);
    	}
    }
}

void cleanup(BelaContext *context, void *userData)
{
	rt_printf("Closing sound files...", recsfinfo.channels);
	sf_close (recsndfile);
}