/* 
 * Linux VeeJay
 *
 * Copyright(C)2002-2004 Niels Elburg <nelburg@looze.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SUPPORT_READ_DV2
#include <libel/rawdv.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libvjmsg/vj-common.h>
#include <libvjmem/vjmem.h>

#include <libdv/dv.h>
#include "avcodec.h"


#define	DV_PAL_SIZE 144000
#define DV_NTSC_SIZE 120000

static void	rawdv_free(dv_t *dv)
{
	if(dv->filename) free(dv->filename);
	if(dv) free(dv);
}

int	rawdv_close(dv_t *dv)
{
	close(dv->fd);
	rawdv_free( dv);
	return 1;
}

#define DV_HEADER_SIZE 120000
dv_t	*rawdv_open_input_file(const char *filename)
{
	struct stat stat_;
	dv_t *dv = (dv_t*)vj_malloc(sizeof(dv_t));
	dv->decoder = dv_decoder_new(1,0,0);
	uint8_t *tmp = (uint8_t*) vj_malloc(sizeof(uint8_t) * DV_HEADER_SIZE);

	dv->fd = open( filename, O_RDONLY );
	
	if(!dv->fd)
	{
		rawdv_free(dv);
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot open '%s'",filename);
		return NULL;
	}
	int n = fstat( dv->fd, &stat_ );

	n = read( dv->fd, tmp, DV_HEADER_SIZE );
	if( n < 0 )
	{
		rawdv_free(dv);
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot read from '%s'", filename);
		return NULL;
	}

	if( dv_parse_header( dv->decoder, tmp) < 0 )
	{
		rawdv_free(dv);
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot parse header");
		return NULL;
	}
	if(dv_is_PAL( dv->decoder ) )
		dv->chunk_size = DV_PAL_SIZE;
	else
		dv->chunk_size = DV_NTSC_SIZE;

	dv->num_frames = (stat_.st_size - 120000) / dv->chunk_size;
	dv->width = dv->decoder->width;
	dv->height = dv->decoder->height;
	dv->audio_rate = dv->decoder->audio->frequency;
	dv->audio_chans = dv->decoder->audio->num_channels;
	dv->audio_qbytes = dv->decoder->audio->quantization;
	dv->fps = ( dv_is_PAL( dv->decoder) ? 25.0 : 29.97 );
	dv->size = dv->decoder->frame_size;
	dv->fmt = ( dv->decoder->sampling == e_dv_sample_422 ? 1 : 0);
	dv->buf = (uint8_t*) vj_malloc(sizeof(uint8_t*) * dv->size);
	if(dv->decoder->sampling == e_dv_sample_411)
	{
		veejay_msg(VEEJAY_MSG_WARNING , "Untested YUV format");
	}


	dv_decoder_free( dv->decoder );
	if(tmp)
		free(tmp);

	if(dv->audio_rate)
	{
		int i;
		for( i = 0; i < 4; i ++ )
		dv->audio_buffers[i] = (int16_t*) vj_malloc(sizeof(int16_t) * 2 * DV_AUDIO_MAX_SAMPLES);
	}


	veejay_msg(VEEJAY_MSG_DEBUG,
		"rawDV: num frames %d, dimensions %d x %d, at %2.2f in %s",
		dv->num_frames,
		dv->width,
		dv->height,
		dv->fps,
		(dv->fmt==1?"422":"420"));
	veejay_msg(VEEJAY_MSG_DEBUG,
		"rawDV: frame size %d, rate %d, channels %d, bits %d",
		dv->size,
		dv->audio_rate,
		dv->audio_chans,
		dv->audio_qbytes);

	return dv;

}


int	rawdv_set_position(dv_t *dv, long nframe)
{
	off_t offset = nframe * dv->size;

	if(nframe <= 0 )
		offset = 0;
	else
		if(nframe > dv->num_frames) 
			offset = dv->num_frames * dv->size;

	if( lseek( dv->fd, offset, SEEK_SET ) < 0 )
		return 0;
	return 1;
}

int	rawdv_read_frame(dv_t *dv, uint8_t *buf )
{
	int pitches[3];
	uint8_t *pixels[3];
	int n = read( dv->fd, dv->buf, dv->size );
	memcpy( buf, dv->buf, dv->size );
	return n;
}

int	rawdv_read_audio_frame(dv_t *dv, uint8_t *audio )
{

	int n = dv_decode_full_audio( dv->decoder, dv->buf, dv->audio_buffers );
	// interleave buffers to audio
	int n_samples = dv_get_num_samples( dv->decoder );
	int n_chans   = dv->audio_chans;
	int16_t *ch0 = dv->audio_buffers[0];
	int16_t *ch1 = dv->audio_buffers[1];
	int i,j;
	for( i = 0; i < n_samples; i ++ )
	{
		*(audio) = *(ch0)  & 0xff;
		*(audio+1) =  (*(ch0) << 8) & 0xff;
		*(audio+2) =  *(ch1) & 0xff;
		*(audio+3) = (*(ch1)<<8) & 0xff;
		*(ch0) ++;
		*(ch1) ++;
		*(audio) += 4;
	}
	return n_samples * 4;
}

int	rawdv_video_frames(dv_t *dv)
{
	return dv->num_frames;
}

int	rawdv_width(dv_t *dv)
{
	return dv->width;
}

int	rawdv_height(dv_t *dv)
{
	return dv->height;
}

double	rawdv_fps(dv_t *dv)
{
	return (double) dv->fps;
}

int	rawdv_compressor(dv_t *dv)
{
	return CODEC_ID_DVVIDEO;
}

char 	*rawdv_video_compressor(dv_t *dv)
{
	char *res = "dvsd\0";
	return res;
}

int	rawdv_audio_channels(dv_t *dv)
{
	return dv->audio_chans;
}

int	rawdv_audio_bits(dv_t *dv)
{
	return dv->audio_qbytes;
}	

int	rawdv_audio_format(dv_t *dv)
{
	// pcm wave format
	return (0x0001);
}

int	rawdv_audio_rate(dv_t *dv)
{
	return dv->audio_rate;
}

int	rawdv_audio_bps(dv_t *dv)
{
	return 4;
}

int	rawdv_frame_size(dv_t *dv)
{
	return dv->size;
}

int	rawdv_interlacing(dv_t *dv)
{
	return 0;
}
#endif