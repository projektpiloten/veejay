/* 
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <elburg@hio.hen.nl> / <nelburg@looze.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 *
 *
 * 05/03/2003: Added XML code from Jeff Carpenter ( jrc@dshome.net )
 * 05/03/2003: Included more sample properties in Jeff's code 
 *	       Create is used to write the Sample to XML, Parse is used to load from XML
 
*/


#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <veejay/vj-global.h>
#include <libsample/sampleadm.h>
#include <libvjmsg/vj-common.h>
#include <libvje/vje.h>
//#include <veejay/vj-lib.h>
//#include <veejay/vj-el.h>
//todo: change this into enum
//#define KAZLIB_OPAQUE_DEBUG 1

#ifdef HAVE_XML2
#endif

#define FOURCC(a,b,c,d) ( (d<<24) | ((c&0xff)<<16) | ((b&0xff)<<8) | (a&0xff) )

#define FOURCC_RIFF     FOURCC ('R', 'I', 'F', 'F')
#define FOURCC_WAVE     FOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT      FOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA     FOURCC ('d', 'a', 't', 'a')


#define VJ_IMAGE_EFFECT_MIN vj_effect_get_min_i()
#define VJ_IMAGE_EFFECT_MAX vj_effect_get_max_i()
#define VJ_VIDEO_EFFECT_MIN vj_effect_get_min_v()
#define VJ_VIDEO_EFFECT_MAX vj_effect_get_max_v()

static int this_sample_id = 0;	/* next available sample id */
static int next_avail_num = 0;	/* available sample id */
static int initialized = 0;	/* whether we are initialized or not */
static hash_t *SampleHash;	/* hash of sample information structs */
static int avail_num[SAMPLE_MAX_SAMPLES];	/* an array of freed sample id's */

static int sampleadm_state = SAMPLE_PEEK;	/* default state */




/****************************************************************************************************
 *
 * sample_size
 *
 * returns current sample_id pointer. size is actually this_sample_id - next_avail_num,
 * but people tend to use size as in length.
 *
 ****************************************************************************************************/
int sample_size()
{
    return this_sample_id;
}

int sample_verify() {
   return hash_verify( SampleHash );
}



/****************************************************************************************************
 *
 * int_hash
 *
 * internal usage. returns hash_val_t for key
 *
 ****************************************************************************************************/
static inline hash_val_t int_hash(const void *key)
{
    return (hash_val_t) key;
}



/****************************************************************************************************
 *
 * int_compare
 *
 * internal usage. compares keys for hash.
 *
 ****************************************************************************************************/
static inline int int_compare(const void *key1, const void *key2)
{
    return ((int) key1 < (int) key2 ? -1 :
	    ((int) key1 > (int) key2 ? +1 : 0));
}

int sample_update(sample_info *sample, int s1) {
  if(s1 <= 0 || s1 >= SAMPLE_MAX_SAMPLES) return 0;
  if(sample) {
    hnode_t *sample_node = hnode_create(sample);
    hnode_put(sample_node, (void*) s1);
    hnode_destroy(sample_node);
    return 1;
  }
  return 0;
}



/****************************************************************************************************
 *
 * sample_init()
 *
 * call before using any other function as sample_skeleton_new
 *
 ****************************************************************************************************/
void sample_init(int len)
{
    if (!initialized) {
	int i;
	for (i = 0; i < SAMPLE_MAX_SAMPLES; i++)
	    avail_num[i] = 0;
	this_sample_id = 1;	/* do not start with zero */
	if (!
	    (SampleHash =
	     hash_create(HASHCOUNT_T_MAX, int_compare, int_hash))) {
	}
	initialized = 1;
    }
}

int sample_set_state(int new_state)
{
    if (new_state == SAMPLE_LOAD || new_state == SAMPLE_RUN
	|| new_state == SAMPLE_PEEK) {
	sampleadm_state = new_state;
    }
    return sampleadm_state;
}

int sample_get_state()
{
    return sampleadm_state;
}

/****************************************************************************************************
 *
 * sample_skeleton_new(long , long)
 *
 * create a new sample, give start and end of new sample. returns sample info block.
 *
 ****************************************************************************************************/

static int _new_id()
{
  /* perhaps we can reclaim a sample id */
	int n;
	int id = 0;
	for (n = 0; n <= next_avail_num; n++)
	{
		if (avail_num[n] != 0)	
		{
			id = avail_num[n];
			avail_num[n] = 0;
			break;
		}
	}
	if( id == 0 )
	{
		if(!this_sample_id) this_sample_id = 1;
		id = this_sample_id;
		this_sample_id ++;
	}
	return id;
}

sample_info *sample_skeleton_new(long startFrame, long endFrame)
{


    sample_info *si;
    int i, j, n, id = 0;

    if (!initialized) {
    	return NULL;
	}
    si = (sample_info *) vj_malloc(sizeof(sample_info));
    if(startFrame < 0) startFrame = 0;
//    if(endFrame <= startFrame&& (endFrame !=0 && startFrame != 0))
	if(endFrame <= startFrame ) 
   {
	veejay_msg(VEEJAY_MSG_ERROR,"End frame %ld must be greater then start frame %ld", startFrame, endFrame);
	return NULL;
    }

    if (!si) {
	return NULL;
    }

    si->sample_id = _new_id();

    snprintf(si->descr,SAMPLE_MAX_DESCR_LEN, "%s", "Untitled");
    for(n=0; n < SAMPLE_MAX_RENDER;n++) {
      si->first_frame[n] = startFrame;
      si->last_frame[n] = endFrame;
	}
    si->speed = 1;
    si->looptype = 1;
    si->max_loops = 0;
    si->next_sample_id = 0;
    si->playmode = 0;
    si->depth = 0;
    si->sub_audio = 0;
    si->audio_volume = 50;
    si->marker_start = 0;
    si->marker_end = 0;
    si->dup = 0;
    si->active_render_entry = 0;
    si->loop_dec = 0;
	si->max_loops2 = 0;
    si->fader_active = 0;
    si->fader_val = 0;
    si->fader_inc = 0;
    si->fader_direction = 0;
    si->rec_total_bytes = 0;
    si->encoder_format = 0;
    si->encoder_base = (char*) vj_malloc(sizeof(char) * 255);
    si->sequence_num = 0;
    si->encoder_duration = 0;
    si->encoder_num_frames = 0;
    si->encoder_destination = (char*) vj_malloc(sizeof(char) * 255);
	si->encoder_succes_frames = 0;
    si->encoder_active = 0;
    si->encoder_total_frames = 0;
    si->rec_total_bytes = 0;
    si->encoder_max_size = 0;
    si->encoder_width = 0;
    si->encoder_height = 0;
    si->encoder_duration = 0;
   // si->encoder_buf = (char*) malloc(sizeof(char) * 10 * 65535 + 16);
    si->auto_switch = 0;
    si->selected_entry = 0;
    si->effect_toggle = 1;
    si->offset = 0;
    si->user_data = NULL;
    sprintf(si->descr, "%s", "Untitled");

    /* the effect chain is initially empty ! */
    for (i = 0; i < SAMPLE_MAX_EFFECTS; i++) {
	
	si->effect_chain[i] =
	    (sample_eff_chain *) vj_malloc(sizeof(sample_eff_chain));
	if (si->effect_chain[i] == NULL) {
		veejay_msg(VEEJAY_MSG_ERROR, "Error allocating entry %d in Effect Chain for new sample",i);
		return NULL;
		}
	si->effect_chain[i]->is_rendering = 0;
	si->effect_chain[i]->effect_id = -1;
	si->effect_chain[i]->e_flag = 0;
	si->effect_chain[i]->frame_offset = -1;
	si->effect_chain[i]->frame_trimmer = 0;
	si->effect_chain[i]->volume = 50;
	si->effect_chain[i]->a_flag = 0;
	si->effect_chain[i]->source_type = 0;
	si->effect_chain[i]->channel = id;	/* with myself by default */
	/* effect parameters initially 0 */
	for (j = 0; j < SAMPLE_MAX_PARAMETERS; j++) {
	    si->effect_chain[i]->arg[j] = 0;
	}

    }
    return si;
}

int sample_store(sample_info * skel)
{
    hnode_t *sample_node;
    if (!skel)
	return -1;
    sample_node = hnode_create(skel);
    if (!sample_node)
	return -1;
    if (!sample_exists(skel->sample_id)) {
	hash_insert(SampleHash, sample_node, (void *) skel->sample_id);
    } else {
	hnode_put(sample_node, (void *) skel->sample_id);
    }
    return 0;
}

/****************************************************************************************************
 *
 * sample_get(int sample_id)
 *
 * returns sample information struct or NULL on error.
 *
 ****************************************************************************************************/
sample_info *sample_get(int sample_id)
{
    sample_info *si;
    //hnode_t *sample_node;
  //  if (!initialized)
//	return NULL;
  //  if (sample_id <= 0)
//	return NULL;
  //  for (i = 0; i <= next_avail_num; i++)
//	if (avail_num[i] == sample_id)
//	    return NULL;
    hnode_t *sample_node = hash_lookup(SampleHash, (void *) sample_id);
    if (sample_node) {
   	 si = (sample_info *) hnode_get(sample_node);
   	 if(si) return si;
	}
    return NULL;
}

/****************************************************************************************************
 *
 * sample_exists(int sample_id)
 *
 * returns 1 if a sample exists in samplehash, or 0 if not.
 *
 ****************************************************************************************************/


int sample_exists(int sample_id) {
	
	hnode_t *sample_node;
	if (!sample_id) return 0;
	
	sample_node = hash_lookup(SampleHash, (void*) sample_id);
	if (!sample_node) {
		return 0;
	}
	
	if(!sample_get(sample_id)) return 0;
	return 1;
}
/*
int sample_exists(int sample_id)
{
    if(sample_id < 1 || sample_id > SAMPLE_MAX_SAMPLES) return 0;
    return (sample_get(sample_id) == NULL ? 0 : 1);
}
*/

int sample_copy(int sample_id)
{
	int i;
	sample_info *org, *copy;
	if (!sample_exists(sample_id))
		return 0;
	org = sample_get(sample_id);
	copy = (sample_info*) vj_malloc(sizeof(sample_info));
	veejay_memcpy( copy,org,sizeof(sample_info));\
	for (i = 0; i < SAMPLE_MAX_EFFECTS; i++)
	{
		copy->effect_chain[i] =
			(sample_eff_chain *) vj_malloc(sizeof(sample_eff_chain));
		if (copy->effect_chain[i] == NULL)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error allocating entry %d in Effect Chain for new sample",i);
			return 0;
		}
		veejay_memcpy( copy->effect_chain[i], org->effect_chain[i], sizeof( sample_eff_chain ) );
	}

	copy->sample_id = _new_id(); 
	if (sample_store(copy) != 0)
		return 0;

	return 1;
}

/****************************************************************************************************
 *
 * sample_get_startFrame(int sample_id)
 *
 * returns first frame of sample.
 *
 ****************************************************************************************************/
int sample_get_longest(int sample_id)
{
	sample_info *si = sample_get(sample_id);
	if(si)
	{
		int len = (si->last_frame[si->active_render_entry] -
			  si->first_frame[si->active_render_entry] );
		int c = 0;
		int tmp = 0;
		int t=0;
		int _id=0;
		int speed = abs(si->speed);
		int duration = len / speed; //how many frames are played of this sample

		if( si->looptype == 2) duration *= 2; // pingpong loop duration     

		for(c=0; c < SAMPLE_MAX_EFFECTS; c++)
		{
			_id = sample_get_chain_channel(sample_id,c);
			t   = sample_get_chain_source(sample_id,c);
	
                        if(t==0 && sample_exists(_id))
			{
				tmp = sample_get_endFrame( _id) - sample_get_startFrame(_id);
				if(tmp>0)
				{
					tmp = tmp / sample_get_speed(_id);
					if(tmp < 0) tmp *= -1;
					if(sample_get_looptype(_id)==2) tmp *= 2; //pingpong loop underlying sample
				}
				if(tmp > duration) duration = tmp; //which one is longer ...	
		        }
		}
		veejay_msg(VEEJAY_MSG_WARNING, "Length of sample in video frames: %ld",duration);
		return duration;
	}
	return 0;
}

int sample_get_startFrame(int sample_id)
{
    sample_info *si = sample_get(sample_id);
    if (si) {
   	if (si->marker_start != 0 && si->marker_end != 0)
		return si->marker_start;
    	else
		return si->first_frame[si->active_render_entry];
	}
    return -1;
}


int	sample_get_el_position( int sample_id, int *start, int *end )
{
	sample_info *si = sample_get(sample_id);
	if(si)
	{
		*start = si->first_frame[ si->active_render_entry ];
		*end   = si->last_frame[ si->active_render_entry ];
		return 1;
	}
	return -1;
}




int sample_get_short_info(int sample_id, int *start, int *end, int *loop, int *speed) {
    sample_info *si = sample_get(sample_id);
    if(si) {
	if(si->marker_start != 0 && si->marker_end !=0) {
	   *start = si->marker_start;
	   *end = si->marker_end;
	} 
        else {
	   *start = si->first_frame[si->active_render_entry];
	   *end = si->last_frame[si->active_render_entry];
	}
        *speed = si->speed;
        *loop = si->looptype;
	return 0;
    }
    *start = 0;
    *end = 0;
    *loop = 0;
    *speed = 0;
    return -1;
}

int sample_entry_is_rendering(int s1, int position) {
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS || position < 0)
	return -1;
    return sample->effect_chain[position]->is_rendering;
}

int sample_entry_set_is_rendering(int s1, int position, int value) {
    sample_info *si = sample_get(s1);
    if (!si)
	return -1;
    if( position >= SAMPLE_MAX_EFFECTS || position < 0) return -1;

    si->effect_chain[position]->is_rendering = value;
    return ( sample_update( si,s1 ));
}


int sample_update_offset(int s1, int n_frame)
{
	int len;
	sample_info *si = sample_get(s1);

	if(!si) return -1;
	si->offset = (n_frame - si->first_frame[si->active_render_entry]);
	len = si->last_frame[si->active_render_entry] - si->first_frame[si->active_render_entry];
	if(si->offset < 0) 
	{	
		veejay_msg(VEEJAY_MSG_WARNING,"Sample bounces outside sample by %d frames",
			si->offset);
		si->offset = 0;
	}
	if(si->offset > len) 
	{
		 veejay_msg(VEEJAY_MSG_WARNING,"Sample bounces outside sample with %d frames",
			si->offset);
		 si->offset = len;
	}
	return ( sample_update(si,s1));
}	

int sample_set_manual_fader( int s1, int value)
{
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  si->fader_active = 2;
  si->fader_val = (float) value;
  si->fader_inc = 0.0;
  si->fader_direction = 0.0;

  /* inconsistency check */
  if(si->effect_toggle == 0) si->effect_toggle = 1;
  return (sample_update(si,s1));

}

int sample_set_fader_active( int s1, int nframes, int direction ) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  if(nframes <= 0) return -1;
  si->fader_active = 1;

  if(direction<0)
	si->fader_val = 255.0;
  else
	si->fader_val = 0.0;

  si->fader_inc = (float) (255.0 / (float) nframes);
  si->fader_direction = direction;
  si->fader_inc *= si->fader_direction;
  /* inconsistency check */
  if(si->effect_toggle == 0)
	{
	si->effect_toggle = 1;
	}
  return (sample_update(si,s1));
}


int sample_reset_fader(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  si->fader_active = 0;
  si->fader_val = 0;
  si->fader_inc = 0;
  return (sample_update(si,s1));
}

int sample_get_fader_active(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  return (si->fader_active);
}

float sample_get_fader_val(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  return (si->fader_val);
}

float sample_get_fader_inc(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  return (si->fader_inc);
}

int sample_get_fader_direction(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  return si->fader_direction;
}

int sample_set_fader_val(int s1, float val) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  si->fader_val = val;
  return (sample_update(si,s1));
}

int sample_apply_fader_inc(int s1) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  si->fader_val += si->fader_inc;
  if(si->fader_val > 255.0 ) si->fader_val = 255.0;
  if(si->fader_val < 0.0 ) si->fader_val = 0.0;
  sample_update(si,s1);
  return (int) (si->fader_val+0.5);
}



int sample_set_fader_inc(int s1, float inc) {
  sample_info *si = sample_get(s1);
  if(!si) return -1;
  si->fader_inc = inc;
  return (sample_update(si,s1));
}

int sample_marker_clear(int sample_id) {
    sample_info *si = sample_get(sample_id);
    if (!si)
	return -1;
    si->marker_start = 0;
    si->marker_end = 0;
    veejay_msg(VEEJAY_MSG_INFO, "Marker cleared (%d - %d) - (speed=%d)",
	si->marker_start, si->marker_end, si->speed);
    return ( sample_update(si,sample_id));
}

int sample_set_marker_start(int sample_id, int marker)
{
    sample_info *si = sample_get(sample_id);
    if (!si)
		return -1;
	if(si->speed < 0 )
	{
		int swap = si->marker_end;
		si->marker_end = marker;
		si->marker_start = swap;
	}
	else
	{
		si->marker_start = marker;
	}
    return ( sample_update(si,sample_id));
}

int sample_set_marker(int sample_id, int start, int end)
{
    sample_info *si = sample_get(sample_id);
    int tmp;
    if(!si) return -1;
    
    if( start < si->first_frame[ si->active_render_entry ] )
		return 0;
	if( start > si->last_frame[ si->active_render_entry ] )
		return 0;
	if( end < si->first_frame[ si->active_render_entry ] )
		return 0;
	if( end > si->last_frame[ si->active_render_entry ] )
		return 0; 

    si->marker_start	= start;
    si->marker_end		= end;
    
    return ( sample_update( si , sample_id ) );	
}

int sample_set_marker_end(int sample_id, int marker)
{
    sample_info *si = sample_get(sample_id);
    if (!si)
		return -1;

	if(si->speed < 0 )
	{
		// mapping in reverse!
		int swap			= si->marker_start;
		si->marker_start	= marker;
		si->marker_end		= swap;
	}
	else
	{
		si->marker_end 		= marker;
	}
	
    return (sample_update(si,sample_id));
}

int sample_set_description(int sample_id, char *description)
{
    sample_info *si = sample_get(sample_id);
    if (!si)
	return -1;
    if (!description || strlen(description) <= 0) {
	snprintf(si->descr, SAMPLE_MAX_DESCR_LEN, "%s", "Untitled");
    } else {
	snprintf(si->descr, SAMPLE_MAX_DESCR_LEN, "%s", description);
    }
    return ( sample_update(si, sample_id)==1 ? 0 : 1);
}

int sample_get_description(int sample_id, char *description)
{
    sample_info *si;
    si = sample_get(sample_id);
    if (!si)
	return -1;
    snprintf(description, SAMPLE_MAX_DESCR_LEN,"%s", si->descr);
    return 0;
}

/****************************************************************************************************
 *
 * sample_get_endFrame(int sample_id)
 *
 * returns last frame of sample.
 *
 ****************************************************************************************************/
int sample_get_endFrame(int sample_id)
{
    sample_info *si = sample_get(sample_id);
    if (si) {
   	if (si->marker_end != 0 && si->marker_start != 0)
		return si->marker_end;
   	 else {
		return si->last_frame[si->active_render_entry];
	}
    }
    return -1;
}
/****************************************************************************************************
 *
 * sample_del( sample_nr )
 *
 * deletes a sample from the hash. returns -1 on error, 1 on success.
 *
 ****************************************************************************************************/
int sample_del(int sample_id)
{
    hnode_t *sample_node;
    sample_info *si;
    si = sample_get(sample_id);
    if (!si)
	return -1;

    sample_node = hash_lookup(SampleHash, (void *) si->sample_id);
    if (sample_node) {
    int i;
    for(i=0; i < SAMPLE_MAX_EFFECTS; i++) 
    {
		if (si->effect_chain[i])
			free(si->effect_chain[i]);
    }
    if (si)
      free(si);
    /* store freed sample_id */
    avail_num[next_avail_num] = sample_id;
    next_avail_num++;
    hash_delete(SampleHash, sample_node);

    return 1;
    }

    return -1;
}


void sample_del_all()
{
    int end = sample_size();
    int i;
    for (i = 0; i < end; i++) {
	if (sample_exists(i)) {
	    sample_del(i);
	}
    }
    this_sample_id = 0;
}

/****************************************************************************************************
 *
 * sample_get_effect( sample_nr , position)
 *
 * returns effect in effect_chain on position X , -1 on error.
 *
 ****************************************************************************************************/
int sample_get_effect(int s1, int position)
{
    sample_info *sample = sample_get(s1);
    if(position >= SAMPLE_MAX_EFFECTS || position < 0 ) return -1;
    if(sample) {
	if(sample->effect_chain[position]->e_flag==0) return -1;
   	return sample->effect_chain[position]->effect_id;
    }
    return -1;
}

int sample_get_effect_any(int s1, int position) {
	sample_info *sample = sample_get(s1);
	if(position >= SAMPLE_MAX_EFFECTS || position < 0 ) return -1;
	if(sample) {
		return sample->effect_chain[position]->effect_id;
	}
	return -1;
}

int sample_get_chain_status(int s1, int position)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->e_flag;
}


int sample_get_offset(int s1, int position)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->frame_offset;
}

int sample_get_trimmer(int s1, int position)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->frame_trimmer;
}

int sample_get_chain_volume(int s1, int position)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->volume;
}

int sample_get_chain_audio(int s1, int position)
{
    sample_info *sample = sample_get(s1);
    if (sample) {
     return sample->effect_chain[position]->a_flag;
    }
    return -1;
}

/****************************************************************************************************
 *
 * sample_get_looptype
 *
 * returns the type of loop set on the sample. 0 on no loop, 1 on ping pong
 * returns -1  on error.
 *
 ****************************************************************************************************/

int sample_get_looptype(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample) {
    	return sample->looptype;
    }
    return 0;
}

int sample_get_playmode(int s1)
{
   sample_info *sample = sample_get(s1);
   if (sample) {
   	 return sample->playmode;
   }
   return -1;
}

/********************
 * get depth: 1 means do what is in underlying sample.
 *******************/
int sample_get_depth(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample)
      return sample->depth;
    return 0;
}

int sample_set_depth(int s1, int n)
{
    sample_info *sample;
    hnode_t *sample_node;

    if (n == 0 || n == 1) {
	sample = sample_get(s1);
	if (!sample)
	    return -1;
	if (sample->depth == n)
	    return 1;
	sample->depth = n;
	sample_node = hnode_create(sample);
	if (!sample_node) {
	    return -1;
	}
	return 1;
    }
    return -1;
}
int sample_set_chain_status(int s1, int position, int status)
{
    sample_info *sample;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    sample->effect_chain[position]->e_flag = status;
    sample_update(sample,s1);
    return 1;
}

/****************************************************************************************************
 *
 * sample_get_speed
 *
 * returns the playback speed set on the sample.
 * returns -1  on error.
 *
 ****************************************************************************************************/
int sample_get_render_entry(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample)
   	 return sample->active_render_entry;
    return 0;
}

int sample_get_speed(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample)
   	 return sample->speed;
    return 0;
}

int sample_get_framedup(int s1) {
	sample_info *sample = sample_get(s1);
	if(sample) return sample->dup;
	return 0;
}


int sample_get_effect_status(int s1)
{
	sample_info *sample = sample_get(s1);
	if(sample) return sample->effect_toggle;
	return 0;
}

/****************************************************************************************************
 *
 * sample_get_effect_arg( sample_nr, position, argnr )
 *
 * returns the required argument set on position X in the effect_chain of sample Y.
 * returns -1 on error.
 ****************************************************************************************************/
int sample_get_effect_arg(int s1, int position, int argnr)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    if (argnr < 0 || argnr > SAMPLE_MAX_PARAMETERS)
	return -1;
    return sample->effect_chain[position]->arg[argnr];
}

int sample_get_selected_entry(int s1) 
{
	sample_info *sample;
	sample = sample_get(s1);
	if(!sample) return -1;
	return sample->selected_entry;
}	

int sample_get_all_effect_arg(int s1, int position, int *args, int arg_len, int n_frame)
{
    int i;
    sample_info *sample;
    sample = sample_get(s1);
    if( arg_len == 0)
	return 1;
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    if (arg_len < 0 || arg_len > SAMPLE_MAX_PARAMETERS)
	return -1;
    for (i = 0; i < arg_len; i++) {
		args[i] = sample->effect_chain[position]->arg[i];
    }
    return i;
}

/********************************************
 * sample_has_extra_frame.
 * return 1 if an effect on the given chain entry 
 * requires another frame, -1 otherwise.
 */
int sample_has_extra_frame(int s1, int position)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    if (sample->effect_chain[position]->effect_id == -1)
	return -1;
    if (vj_effect_get_extra_frame
	(sample->effect_chain[position]->effect_id) == 1)
	return 1;
    return -1;
}

/****************************************************************************************************
 *
 * sample_set_effect_arg
 *
 * sets an argument ARGNR in the chain on position X of sample Y
 * returns -1  on error.
 *
 ****************************************************************************************************/

int sample_set_effect_arg(int s1, int position, int argnr, int value)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position >= SAMPLE_MAX_EFFECTS)
	return -1;
    if (argnr < 0 || argnr > SAMPLE_MAX_PARAMETERS)
	return -1;
    sample->effect_chain[position]->arg[argnr] = value;
    return ( sample_update(sample,s1));
}

int sample_set_selected_entry(int s1, int position) 
{
	sample_info *sample = sample_get(s1);
	if(!sample) return -1;
	if(position< 0 || position >= SAMPLE_MAX_EFFECTS) return -1;
	sample->selected_entry = position;
	return (sample_update(sample,s1));
}

int sample_set_effect_status(int s1, int status)
{
	sample_info *sample = sample_get(s1);
	if(!sample) return -1;
	if(status == 1 || status == 0 )
	{
		sample->effect_toggle = status;
		return ( sample_update(sample,s1));
	}
	return -1;
}

int sample_set_chain_channel(int s1, int position, int input)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    sample->effect_chain[position]->channel = input;
    return ( sample_update(sample,s1));
}

int sample_is_deleted(int s1)
{
    int i;
    for (i = 0; i < next_avail_num; i++) {
	if (avail_num[i] == s1)
	    return 1;
    }
    return 0;
}

int sample_set_chain_source(int s1, int position, int input)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    sample->effect_chain[position]->source_type = input;
    return (sample_update(sample,s1));
}

/****************************************************************************************************
 *
 * sample_set_speed
 *
 * store playback speed in the sample.
 * returns -1  on error.
 *
 ****************************************************************************************************/

int sample_set_user_data(int s1, void *data)
{
	sample_info *sample = sample_get(s1);
	if(!sample) return -1;
	sample->user_data = data;
	return ( sample_update(sample, s1) );
}

void *sample_get_user_data(int s1)
{
	sample_info *sample = sample_get(s1);
	if(!sample) return NULL;
	return sample->user_data;
}

int sample_set_speed(int s1, int speed)
{
    sample_info *sample = sample_get(s1);
    if (!sample) return -1;
	int len = sample->last_frame[sample->active_render_entry] -
			sample->first_frame[sample->active_render_entry];
    if( (speed < -(MAX_SPEED) ) || (speed > MAX_SPEED))
	return -1;
    if( speed > len )
	return -1;
    if( speed < -(len))
	return -1;
    sample->speed = speed;
    return ( sample_update(sample,s1));
}

int sample_set_render_entry(int s1, int entry)
{
    sample_info *sample = sample_get(s1);
    if (!sample) return -1;
    if( entry < 0 || entry >= SAMPLE_MAX_RENDER) return -1;
    sample->active_render_entry = entry;
    return ( sample_update(sample,s1));
}

int sample_set_framedup(int s1, int n) {
	sample_info *sample = sample_get(s1);
	if(!sample) return -1;
	sample->dup = n;
	return ( sample_update(sample,s1));
}

int sample_get_chain_channel(int s1, int position)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->channel;
}

int sample_get_chain_source(int s1, int position)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    return sample->effect_chain[position]->source_type;
}

int sample_get_loops(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample) {
    	return sample->max_loops;
	}
    return -1;
}
int sample_get_loops2(int s1)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    return sample->max_loops2;
}

/****************************************************************************************************
 *
 * sample_set_looptype
 *
 * store looptype in the sample.
 * returns -1  on error.
 *
 ****************************************************************************************************/

int sample_set_looptype(int s1, int looptype)
{
    sample_info *sample = sample_get(s1);
    if(!sample) return -1;

    if (looptype == 0 || looptype == 1 || looptype == 2) {
	sample->looptype = looptype;
	return ( sample_update(sample,s1));
    }
    return -1;
}

int sample_set_playmode(int s1, int playmode)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;

    sample->playmode = playmode;
    return ( sample_update(sample,s1));
}



/*************************************************************************************************
 * update start frame
 *
 *************************************************************************************************/
int sample_set_startframe(int s1, long frame_num)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if(frame_num < 0) return frame_num = 0;
    sample->first_frame[sample->active_render_entry] = frame_num;
    return (sample_update(sample,s1));
}

int sample_set_endframe(int s1, long frame_num)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if(frame_num < 0) return -1;
    sample->last_frame[sample->active_render_entry] = frame_num;
    return (sample_update(sample,s1));
}
int sample_get_next(int s1)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    return sample->next_sample_id;
}
int sample_set_loops(int s1, int nr_of_loops)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    sample->max_loops = nr_of_loops;
    return (sample_update(sample,s1));
}
int sample_set_loops2(int s1, int nr_of_loops)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    sample->max_loops2 = nr_of_loops;
    return (sample_update(sample,s1));
}

int sample_get_sub_audio(int s1)
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    return sample->sub_audio;
}

int sample_set_sub_audio(int s1, int audio)
{
    sample_info *sample = sample_get(s1);
    if(!sample) return -1;
    if (audio < 0 && audio > 1)
	return -1;
    sample->sub_audio = audio;
    return (sample_update(sample,s1));
}

int sample_get_audio_volume(int s1)
{
    sample_info *sample = sample_get(s1);
    if (sample) {
   	 return sample->audio_volume;
	}
   return -1;
}

int sample_set_audio_volume(int s1, int volume)
{
    sample_info *sample = sample_get(s1);
    if (volume < 0)
	volume = 0;
    if (volume > 100)
	volume = 100;
    sample->audio_volume = volume;
    return (sample_update(sample,s1));
}


int sample_set_next(int s1, int next_sample_id)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    /* just add, do not verify 
       on module generation, next sample may not yet be created.
       checks in parameter set in libveejayvj.c
     */
    sample->next_sample_id = next_sample_id;
    return (sample_update(sample,s1));
}

/****************************************************************************************************
 *
 *
 * add a new effect to the chain, returns chain index number on success or -1  if  
 * the requested sample does not exist.
 *
 ****************************************************************************************************/

int sample_chain_malloc(int s1)
{
    sample_info *sample = sample_get(s1);
    int i=0;
    int e_id = 0; 
    int sum =0;
    if (!sample)
	return -1;
    for(i=0; i < SAMPLE_MAX_EFFECTS; i++)
    {
	e_id = sample->effect_chain[i]->effect_id;
	if(e_id)
	{
		if(vj_effect_activate(e_id))
			sum++;
	}
    } 
    veejay_msg(VEEJAY_MSG_INFO, "Allocated %d effects",sum);
    return sum; 
}

int sample_chain_free(int s1)
{
    sample_info *sample = sample_get(s1);
    int i=0;
    int e_id = 0; 
    int sum = 0;
    if (!sample)
	return -1;
    for(i=0; i < SAMPLE_MAX_EFFECTS; i++)
    {
	e_id = sample->effect_chain[i]->effect_id;
	if(e_id!=-1)
	{
		if(vj_effect_initialized(e_id))
		{
			vj_effect_deactivate(e_id);
			sum++;
  		}
	 }
   }  
    return sum;
}

int sample_chain_add(int s1, int c, int effect_nr)
{
    int effect_params = 0, i;
    sample_info *sample = sample_get(s1);
    if (!sample)
		return -1;
    if (c < 0 || c >= SAMPLE_MAX_EFFECTS)
		return -1;
	
	if ( effect_nr < VJ_IMAGE_EFFECT_MIN ) return -1;

	if ( effect_nr > VJ_IMAGE_EFFECT_MAX && effect_nr < VJ_VIDEO_EFFECT_MIN )
		return -1;

	if ( effect_nr > VJ_VIDEO_EFFECT_MAX )
		return -1;	
	
/*
    if(sample->effect_chain[c]->effect_id != -1 &&
		sample->effect_chain[c]->effect_id != effect_nr &&
	  vj_effect_initialized( sample->effect_chain[c]->effect_id ))
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "Effect %s must be freed??", vj_effect_get_description(
			sample->effect_chain[c]->effect_id));
		vj_effect_deactivate( sample->effect_chain[c]->effect_id );
	}
*/
    if( sample->effect_chain[c]->effect_id != -1 && sample->effect_chain[c]->effect_id != effect_nr )
    {
	//verify if the effect should be discarded
	if(vj_effect_initialized( sample->effect_chain[c]->effect_id ))
	{
		// it is using some memory, see if we can free it ...
		int i;
		int ok = 1;
		for(i=(c+1); i < SAMPLE_MAX_EFFECTS; i++)
		{
			if( sample->effect_chain[i]->effect_id == sample->effect_chain[c]->effect_id) ok = 0;
		}
		// ok, lets get rid of it.
		if( ok ) vj_effect_deactivate( sample->effect_chain[c]->effect_id );
	}
    }


    if(!vj_effect_initialized(effect_nr))
    {
	veejay_msg(VEEJAY_MSG_DEBUG, "Effect %s must be initialized now",
		vj_effect_get_description(effect_nr));
	if(!vj_effect_activate( effect_nr ))	return -1;
    }

    sample->effect_chain[c]->effect_id = effect_nr;
    sample->effect_chain[c]->e_flag = 1;	/* effect enabled standard */
    effect_params = vj_effect_get_num_params(effect_nr);
    if (effect_params > 0)
    {
	/* there are parameters, set default values */
	for (i = 0; i < effect_params; i++)
        {
	    int val = vj_effect_get_default(effect_nr, i);
	    sample->effect_chain[c]->arg[i] = val;
	}
    }
    if (vj_effect_get_extra_frame(effect_nr))
   {
    sample->effect_chain[c]->frame_offset = 0;
    sample->effect_chain[c]->frame_trimmer = 0;

    if(s1 > 1)
	 s1 = s1 - 1;
    if(!sample_exists(s1)) s1 = s1 + 1;

	if(sample->effect_chain[c]->channel <= 0)
		sample->effect_chain[c]->channel = s1;
    if(sample->effect_chain[c]->source_type < 0)
		sample->effect_chain[c]->source_type = 0;

        veejay_msg(VEEJAY_MSG_DEBUG,"Effect %s on entry %d overlaying with sample %d",
			vj_effect_get_description(sample->effect_chain[c]->effect_id),c,sample->effect_chain[c]->channel);
    }
    sample_update(sample,s1);

    return c;			/* return position on which it was added */
}

int sample_reset_offset(int s1)
{
	sample_info *sample = sample_get(s1);
	int i;
	if(!sample) return -1;
	for(i=0; i < SAMPLE_MAX_EFFECTS; i++)
	{
		sample->effect_chain[i]->frame_offset = 0;
	}
	return ( sample_update(sample,s1));
}

int sample_set_offset(int s1, int chain_entry, int frame_offset)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    /* set to zero if frame_offset is greater than sample length */
    //if(frame_offset > (sample->last_frame - sample->first_frame)) frame_offset=0;
    sample->effect_chain[chain_entry]->frame_offset = frame_offset;
    return (sample_update(sample,s1));
}

int sample_set_trimmer(int s1, int chain_entry, int trimmer)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    /* set to zero if frame_offset is greater than sample length */
    if (chain_entry < 0 || chain_entry >= SAMPLE_MAX_PARAMETERS)
	return -1;
    if (trimmer > (sample->last_frame[sample->active_render_entry] - sample->first_frame[sample->active_render_entry]))
	trimmer = 0;
    if (trimmer < 0 ) trimmer = 0;
    sample->effect_chain[chain_entry]->frame_trimmer = trimmer;

    return (sample_update(sample,s1));
}
int sample_set_chain_audio(int s1, int chain_entry, int val)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    if (chain_entry < 0 || chain_entry >= SAMPLE_MAX_PARAMETERS)
	return -1;
    sample->effect_chain[chain_entry]->a_flag = val;
    return ( sample_update(sample,s1));
}

int sample_set_chain_volume(int s1, int chain_entry, int volume)
{
    sample_info *sample = sample_get(s1);
    if (!sample)
	return -1;
    /* set to zero if frame_offset is greater than sample length */
    if (volume < 0)
	volume = 100;
    if (volume > 100)
	volume = 0;
    sample->effect_chain[chain_entry]->volume = volume;
    return (sample_update(sample,s1));
}




/****************************************************************************************************
 *
 * sample_chain_clear( sample_nr )
 *
 * clear the entire effect chain.
 * 
 ****************************************************************************************************/

int sample_chain_clear(int s1)
{
    int i, j;
    sample_info *sample = sample_get(s1);

    if (!sample)
	return -1;
    /* the effect chain is gonna be empty! */
    for (i = 0; i < SAMPLE_MAX_EFFECTS; i++) {
	if(sample->effect_chain[i]->effect_id != -1)
	{
		if(vj_effect_initialized( sample->effect_chain[i]->effect_id ))
			vj_effect_deactivate( sample->effect_chain[i]->effect_id ); 
	}
	sample->effect_chain[i]->effect_id = -1;
	sample->effect_chain[i]->frame_offset = -1;
	sample->effect_chain[i]->frame_trimmer = 0;
	sample->effect_chain[i]->volume = 0;
	sample->effect_chain[i]->a_flag = 0;
	sample->effect_chain[i]->source_type = 0;
	sample->effect_chain[i]->channel = s1;
	for (j = 0; j < SAMPLE_MAX_PARAMETERS; j++)
	    sample->effect_chain[i]->arg[j] = 0;
    }

    return (sample_update(sample,s1));
}


/****************************************************************************************************
 *
 * sample_chain_size( sample_nr )
 *
 * returns the number of effects in the effect_chain 
 *
 ****************************************************************************************************/
int sample_chain_size(int s1)
{
    int i, e;

    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    e = 0;
    for (i = 0; i < SAMPLE_MAX_EFFECTS; i++)
	if (sample->effect_chain[i]->effect_id != -1)
	    e++;
    return e;
}

/****************************************************************************************************
 *
 * sample_chain_get_free_entry( sample_nr )
 *
 * returns last available entry 
 *
 ****************************************************************************************************/
int sample_chain_get_free_entry(int s1)
{
    int i;
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    for (i = 0; i < SAMPLE_MAX_EFFECTS; i++)
	if (sample->effect_chain[i]->effect_id == -1)
	    return i;
    return -1;
}


/****************************************************************************************************
 *
 * sample_chain_remove( sample_nr, position )
 *
 * Removes an Effect from the chain of sample <sample_nr> on entry <position>
 *
 ****************************************************************************************************/

static int _sample_can_free(sample_info *sample, int reserved, int effect_id)
{
	int i;
	for(i=0; i < SAMPLE_MAX_EFFECTS; i++)
	{
		if(i != reserved && effect_id == sample->effect_chain[i]->effect_id) return 0;
	}
	return 1;
}

int sample_chain_remove(int s1, int position)
{
    int j;
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
    if (position < 0 || position >= SAMPLE_MAX_EFFECTS)
	return -1;
    if(sample->effect_chain[position]->effect_id != -1)
    {
	if(vj_effect_initialized( sample->effect_chain[position]->effect_id) && 
	 _sample_can_free(sample,position, sample->effect_chain[position]->effect_id))
		vj_effect_deactivate( sample->effect_chain[position]->effect_id);    
    }
    sample->effect_chain[position]->effect_id = -1;
    sample->effect_chain[position]->frame_offset = -1;
    sample->effect_chain[position]->frame_trimmer = 0;
    sample->effect_chain[position]->volume = 0;
    sample->effect_chain[position]->a_flag = 0;
    sample->effect_chain[position]->source_type = 0;
    sample->effect_chain[position]->channel = 0;
    for (j = 0; j < SAMPLE_MAX_PARAMETERS; j++)
	sample->effect_chain[position]->arg[j] = 0;

    return (sample_update(sample,s1));
}

int sample_set_loop_dec(int s1, int active, int periods) {
    sample_info *sample = sample_get(s1);
    if(!sample) return -1;
    if(periods <=0) return -1;
    if(periods > 25) return -1;
    sample->loop_dec = active;
    sample->loop_periods = periods;
    return (sample_update(sample,s1));
}

int sample_get_loop_dec(int s1) {
    sample_info *sample = sample_get(s1);
    if(!sample) return -1;
    return sample->loop_dec;
}

int sample_apply_loop_dec(int s1, double fps) {
    sample_info *sample = sample_get(s1);
    int inc = (int) fps;
    if(!sample) return -1;
    if(sample->loop_dec==1) {
	if( (sample->first_frame[sample->active_render_entry] + inc) >= sample->last_frame[sample->active_render_entry]) {
		sample->first_frame[sample->active_render_entry] = sample->last_frame[sample->active_render_entry]-1;
		sample->loop_dec = 0;
	}
	else {
		sample->first_frame[sample->active_render_entry] += (inc / sample->loop_periods);
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "New starting postions are %ld - %ld",
		sample->first_frame[sample->active_render_entry], sample->last_frame[sample->active_render_entry]);
	return ( sample_update(sample, s1));
    }
    return -1;
}


/* print sample status information into an allocated string str*/
//int sample_chain_sprint_status(int s1, int entry, int changed, int r_changed,char *str,
//			       int frame)
int	sample_chain_sprint_status( int s1,int pfps, int frame, int mode, char *str )
{
    sample_info *sample;
    sample = sample_get(s1);
    if (!sample)
	return -1;
	/*
	fprintf(stderr,
      "%d %d %d %d %d %d %ld %ld %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
	    frame,
	    sample->active_render_entry,
	    r_changed,
	    s1,
	    sample->first_frame[sample->active_render_entry],
	    sample->last_frame[sample->active_render_entry],
	    sample->speed,
	    sample->looptype,
	    sample->max_loops,
	    sample->max_loops2,
	    sample->next_sample_id,
	    sample->depth,
	    sample->playmode,
	    sample->audio_volume,
	    sample->selected_entry,
 	    sample->effect_toggle,
	    changed,
	    vj_effect_real_to_sequence(sample->effect_chain[entry]->effect_id),
				      // effect_id),
	    sample->effect_chain[entry]->e_flag,
	    sample->effect_chain[entry]->frame_offset,
	    sample->effect_chain[entry]->frame_trimmer,
	    sample->effect_chain[entry]->source_type,
	    sample->effect_chain[entry]->channel,
	    this_sample_id - 1);
	*/
/*
	
    sprintf(str,
	    "%d %d %d %d %d %d %ld %ld %d %d %d %d %d %d %d %d %d %d %d %d %d %ld %ld %d %d %d %d %d %d %d %d %d %d %d",
/	    frame,
	    sample->active_render_entry,
	    r_changed,
	    sample->selected_entry,
	    sample->effect_toggle,
	    s1,
	    sample->first_frame[sample->active_render_entry],
	    sample->last_frame[sample->active_render_entry],
	    sample->speed,
	    sample->looptype,
	    sample->max_loops,
	    sample->max_loops2,
	    sample->next_sample_id,
	    sample->depth,
	    sample->playmode,
	    sample->dup,
	    sample->audio_volume,
	    0, 
	    0, 
 	   0,
	    sample->encoder_active,
	    sample->encoder_duration,
	    sample->encoder_succes_frames,
	    sample->auto_switch,
	    changed,
	    vj_effect_real_to_sequence(sample->effect_chain[entry]->effect_id),
				      // effect_id),
	    sample->effect_chain[entry]->e_flag,
	    sample->effect_chain[entry]->frame_offset,
	    sample->effect_chain[entry]->frame_trimmer,
	    sample->effect_chain[entry]->source_type,
	    sample->effect_chain[entry]->channel,
	    sample->effect_chain[entry]->a_flag,
	    sample->effect_chain[entry]->volume,
	    this_sample_id );
    */

	sprintf(str,
		"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		pfps,
		frame,
		mode,
		s1,
		sample->effect_toggle,
		sample->first_frame[ sample->active_render_entry ],
		sample->last_frame[ sample->active_render_entry ],
		sample->speed,
		sample->looptype,
		sample->encoder_active,
		sample->encoder_duration,
		sample->encoder_succes_frames,
		sample_size(),
		sample->marker_start,
		sample->marker_end);
		
		
 
   return 0;
}


#ifdef HAVE_XML2
/*************************************************************************************************
 *
 * UTF8toLAT1()
 *
 * convert an UTF8 string to ISO LATIN 1 string 
 *
 ****************************************************************************************************/
unsigned char *UTF8toLAT1(unsigned char *in)
{
    int in_size, out_size;
    unsigned char *out;

    if (in == NULL)
	return (NULL);

    out_size = in_size = (int) strlen(in) + 1;
    out = malloc((size_t) out_size);

    if (out == NULL) {
	return (NULL);
    }

    if (UTF8Toisolat1(out, &out_size, in, &in_size) != 0)
	{
		//veejay_msg(VEEJAY_MSG_ERROR, "Cannot convert '%s'", in );
		//free(out);
		//return (NULL);
		strncpy( out, in, out_size );
    }

    out = realloc(out, out_size + 1);
    out[out_size] = 0;		/*null terminating out */

    return (out);
}

/*************************************************************************************************
 *
 * ParseArguments()
 *
 * Parse the effect arguments using libxml2
 *
 ****************************************************************************************************/
void ParseArguments(xmlDocPtr doc, xmlNodePtr cur, int *arg)
{
    xmlChar *xmlTemp = NULL;
    unsigned char *chTemp = NULL;
    int argIndex = 0;
    if (cur == NULL)
	return;

    while (cur != NULL && argIndex < SAMPLE_MAX_PARAMETERS) {
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_ARGUMENT))
	{
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		arg[argIndex] = atoi(chTemp);
		argIndex++;
	    }
	    if (xmlTemp)
	   	 xmlFree(xmlTemp);
   	    if (chTemp)
	    	free(chTemp);
	
	}
	// xmlTemp and chTemp should be freed after use
	xmlTemp = NULL;
	chTemp = NULL;
	cur = cur->next;
    }
}


/*************************************************************************************************
 *
 * ParseEffect()
 *
 * Parse an effect using libxml2
 *
 ****************************************************************************************************/
void ParseEffect(xmlDocPtr doc, xmlNodePtr cur, int dst_sample)
{
    xmlChar *xmlTemp = NULL;
    unsigned char *chTemp = NULL;
    int effect_id = -1;
    int arg[SAMPLE_MAX_PARAMETERS];
    int i;
    int source_type = 0;
    int channel = 0;
    int frame_trimmer = 0;
    int frame_offset = 0;
    int e_flag = 0;
    int volume = 0;
    int a_flag = 0;
    int chain_index = 0;

    for (i = 0; i < SAMPLE_MAX_PARAMETERS; i++) {
	arg[i] = 0;
    }

    if (cur == NULL)
	return;


    while (cur != NULL) {
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTID)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		effect_id = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTPOS)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		chain_index = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_ARGUMENTS)) {
	    ParseArguments(doc, cur->xmlChildrenNode, arg);
	}

	/* add source,channel,trimmer,e_flag */
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTSOURCE)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		source_type = atoi(chTemp);
		free(chTemp);
	    }
            if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTCHANNEL)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		channel = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTTRIMMER)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		frame_trimmer = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTOFFSET)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		frame_offset = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECTACTIVE)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		e_flag = atoi(chTemp);
		free(chTemp);
	    } 
	    if(xmlTemp) xmlFree(xmlTemp);
	
	}

	if (!xmlStrcmp
	    (cur->name, (const xmlChar *) XMLTAG_EFFECTAUDIOFLAG)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		a_flag = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	
	}

	if (!xmlStrcmp
	    (cur->name, (const xmlChar *) XMLTAG_EFFECTAUDIOVOLUME)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		volume = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	// xmlTemp and chTemp should be freed after use
	xmlTemp = NULL;
	chTemp = NULL;
	cur = cur->next;
    }


    if (effect_id != -1) {
	int j;
	if (sample_chain_add(dst_sample, chain_index, effect_id) == -1) {
	    veejay_msg(VEEJAY_MSG_ERROR, "Error parsing effect %d (pos %d)\n",
		    effect_id, chain_index);
	}

	/* load the parameter values */
	for (j = 0; j < vj_effect_get_num_params(effect_id); j++) {
	    sample_set_effect_arg(dst_sample, chain_index, j, arg[j]);
	}
	sample_set_chain_channel(dst_sample, chain_index, channel);
	sample_set_chain_source(dst_sample, chain_index, source_type);

	/* set other parameters */
	if (a_flag) {
	    sample_set_chain_audio(dst_sample, chain_index, a_flag);
	    sample_set_chain_volume(dst_sample, chain_index, volume);
	}

	sample_set_chain_status(dst_sample, chain_index, e_flag);

	sample_set_offset(dst_sample, chain_index, frame_offset);
	sample_set_trimmer(dst_sample, chain_index, frame_trimmer);
    }

}

/*************************************************************************************************
 *
 * ParseEffect()
 *
 * Parse the effects array 
 *
 ****************************************************************************************************/
void ParseEffects(xmlDocPtr doc, xmlNodePtr cur, sample_info * skel)
{
    int effectIndex = 0;
    while (cur != NULL && effectIndex < SAMPLE_MAX_EFFECTS) {
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_EFFECT)) {
	    ParseEffect(doc, cur->xmlChildrenNode, skel->sample_id);
		effectIndex++;
	}
	//effectIndex++;
	cur = cur->next;
    }
}

/*************************************************************************************************
 *
 * ParseSample()
 *
 * Parse a sample
 *
 ****************************************************************************************************/
void ParseSample(xmlDocPtr doc, xmlNodePtr cur, sample_info * skel)
{

    xmlChar *xmlTemp = NULL;
    unsigned char *chTemp = NULL;

    while (cur != NULL) {
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_SAMPLEID)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		skel->sample_id = atoi(chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_CHAIN_ENABLED))
	{
		xmlTemp = xmlNodeListGetString( doc, cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1( xmlTemp );
		if(chTemp)
		{
			skel->effect_toggle = atoi(chTemp);
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_SAMPLEDESCR)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		snprintf(skel->descr, SAMPLE_MAX_DESCR_LEN,"%s", chTemp);
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_FIRSTFRAME)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_startframe(skel->sample_id, atol(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_VOL)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_audio_volume(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);

	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_LASTFRAME)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_endframe(skel->sample_id, atol(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_SPEED)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_speed(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_FRAMEDUP)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp)
		{
			sample_set_framedup(skel->sample_id, atoi(chTemp));
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_LOOPTYPE)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_looptype(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_MAXLOOPS)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_loops(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_NEXTSAMPLE)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_next(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_DEPTH)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_depth(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_PLAYMODE)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_playmode(skel->sample_id, atoi(chTemp));
	        free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name,(const xmlChar *) XMLTAG_FADER_ACTIVE)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp) {
			skel->fader_active = atoi(chTemp);
		        free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name,(const xmlChar *) XMLTAG_FADER_VAL)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp){
			skel->fader_val = atoi(chTemp);
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name,(const xmlChar*) XMLTAG_FADER_INC)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp) {
			skel->fader_inc = atof(chTemp);
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}

	if (!xmlStrcmp(cur->name,(const xmlChar*) XMLTAG_FADER_DIRECTION)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp) {
			skel->fader_inc = atoi(chTemp);
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}
	if(!xmlStrcmp(cur->name,(const xmlChar*) XMLTAG_LASTENTRY)) {
		xmlTemp = xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		chTemp = UTF8toLAT1(xmlTemp);
		if(chTemp) {
			skel->selected_entry = atoi(chTemp);
			free(chTemp);
		}
		if(xmlTemp) xmlFree(xmlTemp);
	}	
	/*
	   if (!xmlStrcmp(cur->name, (const xmlChar *)XMLTAG_VOLUME)) {
	   xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	   chTemp = UTF8toLAT1( xmlTemp );
	   if( chTemp ){
	   //sample_set_volume(skel->sample_id, atoi(chTemp ));
	   }
	   }
	   if (!xmlStrcmp(cur->name, (const xmlChar *)XMLTAG_SUBAUDIO)) {
	   xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	   chTemp = UTF8toLAT1( xmlTemp );
	   if( chTemp ){
	   sample_set_sub_audio(skel->sample_id, atoi(chTemp ));
	   }
	   }
	 */
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_MARKERSTART)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_marker_start(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);
	}
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_MARKEREND)) {
	    xmlTemp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	    chTemp = UTF8toLAT1(xmlTemp);
	    if (chTemp) {
		sample_set_marker_end(skel->sample_id, atoi(chTemp));
		free(chTemp);
	    }
	    if(xmlTemp) xmlFree(xmlTemp);

	}

	ParseEffects(doc, cur->xmlChildrenNode, skel);

	// xmlTemp and chTemp should be freed after use
	xmlTemp = NULL;
	chTemp = NULL;

	cur = cur->next;
    }
    return;
}


/****************************************************************************************************
 *
 * sample_readFromFile( filename )
 *
 * load samples and effect chain from an xml file. 
 *
 ****************************************************************************************************/
int sample_readFromFile(char *sampleFile)
{
    xmlDocPtr doc;
    xmlNodePtr cur;
    sample_info *skel;

    /*
     * build an XML tree from a the file;
     */
    doc = xmlParseFile(sampleFile);
    if (doc == NULL) {
	return (0);
    }

    /*
     * Check the document is of the right kind
     */

    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
	veejay_msg(VEEJAY_MSG_ERROR,"Empty samplelist. Nothing to do.\n");
	xmlFreeDoc(doc);
	return (0);
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_SAMPLES)) {
	veejay_msg(VEEJAY_MSG_ERROR, "This is not a samplelist: %s",
		XMLTAG_SAMPLES);
	xmlFreeDoc(doc);
	return (0);
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
	if (!xmlStrcmp(cur->name, (const xmlChar *) XMLTAG_SAMPLE)) {
	    skel = sample_skeleton_new(0, 1);
	    sample_store(skel);
	    if (skel != NULL) {
		ParseSample(doc, cur->xmlChildrenNode, skel);
	    }
	}
	cur = cur->next;
    }
    xmlFreeDoc(doc);

    return (1);
}

void CreateArguments(xmlNodePtr node, int *arg, int argcount)
{
    int i;
    char buffer[100];
    argcount = SAMPLE_MAX_PARAMETERS;
    for (i = 0; i < argcount; i++) {
	//if (arg[i]) {
	    sprintf(buffer, "%d", arg[i]);
	    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_ARGUMENT,
			(const xmlChar *) buffer);
	//}
    }
}


void CreateEffect(xmlNodePtr node, sample_eff_chain * effect, int position)
{
    char buffer[100];
    xmlNodePtr childnode;

    sprintf(buffer, "%d", position);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTPOS,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->effect_id);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTID,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->e_flag);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTACTIVE,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->source_type);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTSOURCE,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->channel);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTCHANNEL,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->frame_offset);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTOFFSET,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->frame_trimmer);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTTRIMMER,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->a_flag);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTAUDIOFLAG,
		(const xmlChar *) buffer);

    sprintf(buffer, "%d", effect->volume);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTAUDIOVOLUME,
		(const xmlChar *) buffer);


    childnode =
	xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_ARGUMENTS, NULL);
    CreateArguments(childnode, effect->arg,
		    vj_effect_get_num_params(effect->effect_id));

    
}




void CreateEffects(xmlNodePtr node, sample_eff_chain ** effects)
{
    int i;
    xmlNodePtr childnode;

    for (i = 0; i < SAMPLE_MAX_EFFECTS; i++) {
	if (effects[i]->effect_id != -1) {
	    childnode =
		xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECT,
			    NULL);
	    CreateEffect(childnode, effects[i], i);
	}
    }
    
}

void CreateSample(xmlNodePtr node, sample_info * sample)
{
    char buffer[100];
    xmlNodePtr childnode;

    sprintf(buffer, "%d", sample->sample_id);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_SAMPLEID,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->effect_toggle);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_CHAIN_ENABLED,
		(const xmlChar *) buffer);


    sprintf(buffer,"%d", sample->active_render_entry);
    xmlNewChild(node,NULL,(const xmlChar*) XMLTAG_RENDER_ENTRY,(const xmlChar*)buffer);
    sprintf(buffer, "%s", sample->descr);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_SAMPLEDESCR,
		(const xmlChar *) buffer);
    sprintf(buffer, "%ld", sample->first_frame[sample->active_render_entry]);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_FIRSTFRAME,
		(const xmlChar *) buffer);
    sprintf(buffer, "%ld", sample->last_frame[sample->active_render_entry]);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_LASTFRAME,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->speed);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_SPEED,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->dup);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_FRAMEDUP,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->looptype);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_LOOPTYPE,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->max_loops);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_MAXLOOPS,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->next_sample_id);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_NEXTSAMPLE,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->depth);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_DEPTH,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->playmode);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_PLAYMODE,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->audio_volume);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_VOL,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->marker_start);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_MARKERSTART,
		(const xmlChar *) buffer);
    sprintf(buffer, "%d", sample->marker_end);
    xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_MARKEREND,
		(const xmlChar *) buffer);

	sprintf(buffer,"%d",sample->fader_active);
	xmlNewChild(node,NULL,(const xmlChar *) XMLTAG_FADER_ACTIVE,
		(const xmlChar *) buffer);
	sprintf(buffer,"%f",sample->fader_inc);
	xmlNewChild(node,NULL,(const xmlChar *) XMLTAG_FADER_INC,
		(const xmlChar *) buffer);
	sprintf(buffer,"%f",sample->fader_val);
	xmlNewChild(node,NULL,(const xmlChar *) XMLTAG_FADER_VAL,
		(const xmlChar *) buffer);
	sprintf(buffer,"%d",sample->fader_direction);
	xmlNewChild(node,NULL,(const xmlChar *) XMLTAG_FADER_DIRECTION,
		(const xmlChar *) buffer);
	sprintf(buffer,"%d",sample->selected_entry);
	xmlNewChild(node,NULL,(const xmlChar *) XMLTAG_LASTENTRY,
		(const xmlChar *)buffer);
    childnode =
	xmlNewChild(node, NULL, (const xmlChar *) XMLTAG_EFFECTS, NULL);

    
    
    CreateEffects(childnode, sample->effect_chain);

}

/****************************************************************************************************
 *
 * sample_writeToFile( filename )
 *
 * writes all sample info to a file. 
 *
 ****************************************************************************************************/
int sample_writeToFile(char *sampleFile)
{
    int i;
	char *encoding = "UTF-8";	
    sample_info *next_sample;
    xmlDocPtr doc;
    xmlNodePtr rootnode, childnode;

    doc = xmlNewDoc("1.0");
    rootnode =
	xmlNewDocNode(doc, NULL, (const xmlChar *) XMLTAG_SAMPLES, NULL);
    xmlDocSetRootElement(doc, rootnode);
    for (i = 1; i < sample_size(); i++) {
	next_sample = sample_get(i);
	if (next_sample) {
	    childnode =
		xmlNewChild(rootnode, NULL,
			    (const xmlChar *) XMLTAG_SAMPLE, NULL);
	    CreateSample(childnode, next_sample);
	}
    }
    //xmlSaveFormatFile(sampleFile, doc, 1);
	xmlSaveFormatFileEnc( sampleFile, doc, encoding, 1 );
    xmlFreeDoc(doc);

    return 1;
}
#endif
