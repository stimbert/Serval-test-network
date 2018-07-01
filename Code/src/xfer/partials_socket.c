/*
Serval Low-bandwidth asychronous Rhizome Demonstrator.
Copyright (C) 2015 Serval Project Inc.

This program monitors a local Rhizome database and attempts
to synchronise it over low-bandwidth declarative transports, 
such as bluetooth name or wifi-direct service information
messages.  It is intended to give a high priority to MeshMS
converations among nearby nodes.

The design is fully asynchronous, so a call to the update_my_message()
function from time to time should be all that is required.


This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <assert.h>

#include "sync.h"
#include "lbard.h"
#include "send_all.h"
#define RCVSIZE 1494
#define BUFFERSIZE 8192


int partial_recent_sender_report(struct partial_bundle *p)
{
  char socketMessage[BUFFERSIZE];
  //fprintf(stderr,"Recent senders for bundle %c%c%c%c*/%lld:\n",
  //  p->bid_prefix[0],p->bid_prefix[1],p->bid_prefix[2],p->bid_prefix[3],
	//  p->bundle_version);

    snprintf(socketMessage, BUFFERSIZE, "Recent senders for bundle %c%c%c%c*/%lld:\n",
             p->bid_prefix[0], p->bid_prefix[1], p->bid_prefix[2], p->bid_prefix[3],
             p->bundle_version);
    send_all(socketMessage);

    int i;
    time_t t = time(0);

    memset(socketMessage, 0, BUFFERSIZE);
    for (i = 0; i < MAX_RECENT_SENDERS; i++)
      if ((t - p->senders.r[i].last_time) < 10)
      {
        /*
      fprintf(stderr,"  #%02d : %02X%02X* (T-%d sec)\n",
	      i,p->senders.r[i].sid_prefix[0],p->senders.r[i].sid_prefix[1],
	      (int)(t-p->senders.r[i].last_time));
        */
        snprintf(socketMessage, BUFFERSIZE, "  #%02d : %02X%02X* (T-%d sec)\n",
                 i, p->senders.r[i].sid_prefix[0], p->senders.r[i].sid_prefix[1],
                 (int)(t - p->senders.r[i].last_time));
        send_all(socketMessage);
      }
    dump_partial(p);
    return 0;
}

int partial_update_recent_senders(struct partial_bundle *p, char *sender_prefix_hex)
{
  // Get peer SID prefix as HEX
  unsigned char sender_prefix_bin[2];
  for (int i = 0; i < 2; i++)
    sender_prefix_bin[i] = hex_to_val(sender_prefix_hex[i * 2 + 1]) + hex_to_val(sender_prefix_hex[i * 2 + 0]) * 16;

  int free_slot = random() % MAX_RECENT_SENDERS;
  int index = 0;
  time_t t = time(0);
  for (index = 0; index < MAX_RECENT_SENDERS; index++)
  {
    if ((sender_prefix_bin[0] == p->senders.r[index].sid_prefix[0]) && (sender_prefix_bin[1] == p->senders.r[index].sid_prefix[1]))
    {
      break;
    }
    if ((t - p->senders.r[index].last_time) >= 30)
      free_slot = index;
  }
  if (index == MAX_RECENT_SENDERS)
    index = free_slot;

  // Update record
  p->senders.r[index].sid_prefix[0] = sender_prefix_bin[0];
  p->senders.r[index].sid_prefix[1] = sender_prefix_bin[1];
  p->senders.r[index].last_time = time(0);

  partial_recent_sender_report(p);

  return -1;
}

int clear_partial(struct partial_bundle *p)
{
  while (p->manifest_segments)
  {
    struct segment_list *s = p->manifest_segments;
    p->manifest_segments = s->next;
    if (s->data)
      free(s->data);
    s->data = NULL;
    free(s);
    s = NULL;
  }
  while (p->body_segments)
  {
    struct segment_list *s = p->body_segments;
    p->body_segments = s->next;
    if (s->data)
      free(s->data);
    s->data = NULL;
    free(s);
    s = NULL;
  }

  bzero(p, sizeof(struct partial_bundle));
  return -1;
}

int dump_segment_list(struct segment_list *s)
{
  char socketMessage[BUFFERSIZE];
  if (!s)
    return 0;
  while (s)
  {
    //fprintf(stderr,"    [%d,%d)\n",s->start_offset,s->start_offset+s->length);
    snprintf(socketMessage, BUFFERSIZE, "    [%d,%d)\n", s->start_offset, s->start_offset + s->length);
    send_all(socketMessage);
    s = s->next;
  }
  return 0;
}

int dump_partial(struct partial_bundle *p)
{
  char socketMessage[BUFFERSIZE];
  /*
  printf(">>> %s Progress receiving BID=%s* version %lld: "
	 "manifest is %d bytes long (RX bitmap %02x%02x), and body %d bytes long. Bitmap p=%5d : ",
	 timestamp_str(),p->bid_prefix,p->bundle_version,
         p->manifest_length,p->request_manifest_bitmap[0],p->request_manifest_bitmap[1],
	 p->body_length,
	 p->request_bitmap_start
	 );
   */
  snprintf(socketMessage, BUFFERSIZE, ">>> %s Progress receiving BID=%s* version %lld: "
                                      "manifest is %d bytes long (RX bitmap %02x%02x), and body %d bytes long. Bitmap p=%5d : ",
           timestamp_str(), p->bid_prefix, p->bundle_version,
           p->manifest_length, p->request_manifest_bitmap[0], p->request_manifest_bitmap[1],
           p->body_length,
           p->request_bitmap_start);
  send_all(socketMessage);
  int max_block = 256;
  if (p->body_length > -1)
  {
    max_block = (p->body_length - p->request_bitmap_start);
    if (max_block & 0x3f)
      max_block = 1 + max_block / 64;
    else
      max_block = 0 + max_block / 64;
  }
  if (max_block > 256)
    max_block = 256;
  dump_progress_bitmap(stdout, p->request_bitmap, max_block);

  if (0)
  {
    //fprintf(stderr, "  Manifest pieces received:\n");
    snprintf(socketMessage, BUFFERSIZE, "  Manifest pieces received:\n");
    send_all(socketMessage);
    dump_segment_list(p->manifest_segments);
    //fprintf(stderr, "  Body pieces received:\n");
    snprintf(socketMessage, BUFFERSIZE, "  Body pieces received:\n");
    send_all(socketMessage);
    dump_segment_list(p->body_segments);
    //fprintf(stderr, "  Request bitmap: start=%d, bits=\n    ",
    //        p->request_bitmap_start);
    snprintf(socketMessage, BUFFERSIZE, "  Request bitmap: start=%d, bits=\n    ",
             p->request_bitmap_start);
    send_all(socketMessage);
  }
  return 0;
}

int merge_segments(struct segment_list **s)
{
  char socketMessage[BUFFERSIZE];
  if (!s)
    return -1;
  if (!(*s))
    return -1;

  // Segments are sorted in descending order
  while ((*s) && (*s)->next)
  {
    struct segment_list *me = *s;
    struct segment_list *next = (*s)->next;
    if (me->start_offset <= (next->start_offset + next->length))
    {
      // Merge this piece onto the end of the next piece
      if (debug_pieces)
      {
        /*
        printf("Merging [%d..%d) and [%d..%d)\n",
               me->start_offset, me->start_offset + me->length,
               next->start_offset, next->start_offset + next->length);
        */
        snprintf(socketMessage, BUFFERSIZE, "Merging [%d..%d) and [%d..%d)\n",
                 me->start_offset, me->start_offset + me->length,
                 next->start_offset, next->start_offset + next->length);
        send_all(socketMessage);
      }
      int extra_bytes = (me->start_offset + me->length) - (next->start_offset + next->length);
      int new_length = next->length + extra_bytes;
      next->data = realloc(next->data, new_length);
      assert(next->data);
      bcopy(&me->data[me->length - extra_bytes], &next->data[next->length],
            extra_bytes);
      next->length = new_length;

      // Excise redundant segment from list
      *s = next;
      next->prev = me->prev;
      if (me->prev)
        me->prev->next = next;

      // Free redundant segment.
      free(me->data);
      me->data = NULL;
      free(me);
      me = NULL;
    }
    else
      s = &(*s)->next;
  }
  return 0;
}

/* Find the first byte missing in the following segment list.
   Basically this boils down to being either byte 0, or the
   first byte after the first segment. 

   However, we actually want to randomise the byte we ask for,
   so that if a peer is sending to multiple peers, that we can
   encourage them to send unique content.  This ideally requires
   that we know who a piece is addressed to. But in the very
   least, we should pick a random starting point that is adjacent
   to one of our partial pieces.  However, we need to take care to
   not make the sender think that we have it all.
*/
int partial_find_missing_byte(struct segment_list *s, int *isFirstMissingByte)
{
  int add_zero = 1;

  // We limit ourselves to the first 16 gaps, so that we can tend to complete
  // reception of earlier parts of a bundle before proceeding to later parts.
  // This reduces the complexity of the job of receiving, by tending to reduce
  // the number of segments of received material at any point in time.
  int candidates[16];
  int candidate_count = 0;

  // Walk the segment list. Adjacent segments should be merged,
  // so the offset following each segment is a valid candidate,
  // except if a candidate is the end of the file.
  while (s)
  {
    if (!s->start_offset)
      add_zero = 0;
    if (candidate_count < 16)
      candidates[candidate_count++] = s->start_offset + s->length;
    s = s->next;
  }
  if ((candidate_count < 16) && (add_zero))
    candidates[candidate_count++] = 0;

  // The values should be in descending order. Don't ask for highest value,
  // incase it signals the end of the bundle (we don't necessarily know the
  // payload length during reception).  Thus only ask for the end point if
  // there are no other alternatives.
  int selection = candidate_count - 1;
  if (selection < 0)
    selection = 0;

  if ((!(option_flags & FLAG_NO_RANDOMIZE_REDIRECT_OFFSET)) && (candidate_count > 1))
    selection = random() % (candidate_count - 1);

  if (isFirstMissingByte)
  {
    *isFirstMissingByte = 0;
    if (selection == (candidate_count - 1))
      *isFirstMissingByte = 1;
  }

  return candidates[selection] & 0xffffffc0;
}
