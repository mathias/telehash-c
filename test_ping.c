#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "util.h"
#include "crypt.h"
#include "hn.h"
#include "path.h"

int main(void)
{
  unsigned char out[4096], hn[64];
  packet_t p,p2;
  hn_t id, *seeds;
  path_t path;

  crypt_init();
  hn_init();

  p = packet_new();
  printf("empty packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  packet_json(p,(unsigned char*)"{\"foo\":\"bar\"}",strlen("{\"foo\":\"bar\"}"));
  packet_body(p,(unsigned char*)"BODY",4);
  printf("full packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  p2 = packet_copy(p);
  printf("copy packet %s\n",util_hex(packet_raw(p2),packet_len(p2),out));

  id = hn_getfile("id.json");
  if(!id)
  {
    printf("failed to load id.json: %s\n", crypt_err());
    return -1;
  }
  printf("loaded hashname %.*s\n",64,util_hex(id->hashname,32,hn));

  seeds = hn_getsfile("seeds.json");
  if(!*seeds)
  {
    printf("failed to load seeds.json: %s\n", crypt_err());
    return -1;
  }
  printf("loaded seed %.*s %s\n",64,util_hex((*seeds)->hashname,32,hn), path_json((*seeds)->paths[0]));

  path = path_new("ipv4");
  path_ip(path,"127.0.0.1");
  path_port(path, 42424);
  printf("path %s\n",path_json(path));

  return 0;
}
