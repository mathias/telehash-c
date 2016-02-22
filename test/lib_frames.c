#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  util_frames_t frames;

  frames = util_frames_new(16);
  fail_unless(frames);
  fail_unless(util_frames_inlen(frames) == 0);
  fail_unless(util_frames_outlen(frames) == 0);
  fail_unless(!util_frames_inbox(frames,NULL));
  fail_unless(!util_frames_outbox(frames,NULL));
  fail_unless(!util_frames_free(frames));

  // try a test packet
  frames = util_frames_new(16);
  lob_t packet = lob_new();
  lob_body(packet,0,100);
  fail_unless(util_frames_send(frames, lob_copy(packet)));
  fail_unless(util_frames_outlen(frames) == 102);
  fail_unless(util_frames_outbox(frames,NULL));

  // cause a flush frame
  uint8_t frame[16];
  fail_unless(util_frames_send(frames,NULL));
  fail_unless(util_frames_outbox(frames,frame));
  printf("frame %s\n",util_hex(frame,16,NULL));
  fail_unless(strcmp("2a0000002a00000000000000daa1a223",util_hex(frame,16,NULL)) == 0);

  // receive the flush frame
  fail_unless(util_frames_inbox(frames,frame));

  // cause a data frame
  fail_unless(util_frames_outbox(frames,frame));
  printf("frame %s\n",util_hex(frame,16,NULL));
  fail_unless(strcmp("000000000000000000000000611441d9",util_hex(frame,16,NULL)) == 0);
  
  // receive the data frame
  fail_unless(util_frames_inbox(frames,frame));
  fail_unless(util_frames_inlen(frames) == 12);

  // do rest
  while(util_frames_outbox(frames,frame)) fail_unless(util_frames_inbox(frames,frame));
  
  fail_unless(util_frames_outlen(frames) == 0);
  fail_unless(util_frames_inbox(frames,NULL));
  lob_t in = util_frames_receive(frames);
  fail_unless(in);
  fail_unless(!util_frames_inbox(frames,NULL));
  fail_unless(in->body_len == 100);

  fail_unless(!util_frames_free(frames));

  util_frames_t fa = util_frames_new(64);
  util_frames_t fb = util_frames_new(64);
  lob_t msg = lob_new();
  lob_body(msg, NULL, 1024);
  e3x_rand(msg->body, 1024);
  fail_unless(!util_frames_outbox(fa,NULL));
  fail_unless(!util_frames_inbox(fb,NULL));
  util_frames_send(fa,msg);

  // chat
  uint8_t f64[64];
  while(util_frames_outbox(fa,f64))
  {
    fail_unless(util_frames_inbox(fb,f64));
    if(util_frames_outbox(fb,f64)) fail_unless(util_frames_inbox(fa,f64));
  }

  fail_unless(!util_frames_outbox(fa,NULL));
  lob_t msg2 = util_frames_receive(fb);
  fail_unless(msg2);
  fail_unless(msg2->body_len == 1024);

  fail_unless(!util_frames_free(fa));
  fail_unless(!util_frames_free(fb));
  
  return 0;
}
