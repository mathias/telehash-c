#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "telehash.h"
#include "tmesh.h"

//////////////////
// private community management methods

// find an epoch to send it to in this community
static void cmnty_send(pipe_t pipe, lob_t packet, link_t link)
{
  cmnty_t c;
  mote_t m;
  if(!pipe || !pipe->arg || !packet || !link)
  {
    LOG("bad args");
    lob_free(packet);
    return;
  }
  c = (cmnty_t)pipe->arg;

  // find link in this community w/ tx epoch
  for(m=c->motes;m;m=m->next) if(m->link == link)
  {
    util_chunks_send(m->chunks, packet);
    return;
  }

  LOG("no link in community");
  lob_free(packet);
}

static cmnty_t cmnty_free(cmnty_t c)
{
  if(!c) return NULL;
  // do not free c->name, it's part of c->pipe
  // TODO free motes
  pipe_free(c->pipe);
  free(c);
  return NULL;
}

// create a new blank community
static cmnty_t cmnty_new(tmesh_t tm, char *medium, char *name)
{
  cmnty_t c;
  uint8_t bin[5];
  if(!tm || !name || !medium || strlen(medium) != 8) return LOG("bad args");
  if(base32_decode(medium,0,bin,5) != 5) return LOG("bad medium encoding: %s",medium);
  if(!medium_check(tm,bin)) return LOG("unknown medium %s",medium);

  // note, do we need to be paranoid and make sure name is not a duplicate?

  if(!(c = malloc(sizeof (struct cmnty_struct)))) return LOG("OOM");
  memset(c,0,sizeof (struct cmnty_struct));

  if(!(c->medium = medium_get(tm,bin))) return cmnty_free(c);
  if(!(c->pipe = pipe_new("tmesh"))) return cmnty_free(c);
  c->tm = tm;
  c->type = (c->medium->bin[0] > 128) ? PRIVATE : PUBLIC;
  c->next = tm->coms;
  tm->coms = c;

  // set up pipe
  c->pipe->arg = c;
  c->name = c->pipe->id = strdup(name);
  c->pipe->send = cmnty_send;

  return c;
}


//////////////////
// public methods

// join a new private/public community
cmnty_t tmesh_join(tmesh_t tm, char *medium, char *name)
{
  epoch_t ping, echo;
  uint8_t roll[64];
  struct knock_struct ktmp = {0,0,0,0,0,0,0};
  cmnty_t c = cmnty_new(tm,medium,name);
  if(!c) return LOG("bad args");

  if(c->type == PUBLIC)
  {
    if(!(c->sync = mote_new(NULL))) return cmnty_free(c);
    if(!(ping = c->sync->epochs = epoch_new(1))) return cmnty_free(c);
    if(!(echo = ping->next = epoch_new(1))) return cmnty_free(c);
    ping->type = PING;
    echo->type = ECHO;
    e3x_hash(c->medium->bin,5,roll);
    e3x_hash((uint8_t*)name,strlen(name),roll+32);
    e3x_hash(roll,64,ping->secret);

    // cache ping channel for faster detection
    ktmp.com = c;
    mote_knock(c->sync,&ktmp,0);
    c->sync->ping = ktmp.chan;

    // generate public intermediate keys packet
    if(!tm->pubim) tm->pubim = hashname_im(tm->mesh->keys, hashname_id(tm->mesh->keys,tm->mesh->keys));
    
  }else{
    c->pipe->path = lob_new();
    lob_set(c->pipe->path,"type","tmesh");
    lob_set(c->pipe->path,"medium",medium);
    lob_set(c->pipe->path,"name",name);
    tm->mesh->paths = lob_push(tm->mesh->paths, c->pipe->path);
    
  }
  

  return c;
}

// add a link known to be in this community to look for
mote_t tmesh_link(tmesh_t tm, cmnty_t c, link_t link)
{
  uint8_t roll[64];
  mote_t m;
  struct knock_struct ktmp = {0,0,0,0,0,0,0};
  if(!tm || !c || !link) return LOG("bad args");

  // check list of motes, add if not there
  for(m=c->motes;m;m = m->next) if(m->link == link) return m;

  if(!(m = mote_new(link))) return LOG("OOM");
  if(!(m->epochs = epoch_new(0))) return mote_free(m);
  m->epochs->type = PING;
  m->next = c->motes;
  c->motes = m;
  
  // generate mote-specific ping secret by combining community one with it
  memcpy(roll,c->sync->epochs->secret,32);
  memcpy(roll+32,link->id->bin,32);
  e3x_hash(roll,64,m->epochs->secret);

  ktmp.com = c;
  mote_knock(m,&ktmp,0);
  m->ping = ktmp.chan;

  return m;
}

pipe_t tmesh_on_path(link_t link, lob_t path)
{
//  cmnty_t c;
  tmesh_t tm;
//  epoch_t e;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(tm = xht_get(link->mesh->index, "tmesh"))) return NULL;
  if(util_cmp("tmesh",lob_get(path,"type"))) return NULL;
  // TODO, check for community match and add
  // or create direct?
  return NULL;
}

// handle incoming packets for the built-in map channel
void tmesh_map_handler(link_t link, e3x_channel_t chan, void *arg)
{
  lob_t packet;
//  mote_t mote = arg;
  if(!link || !arg) return;

  while((packet = e3x_channel_receiving(chan)))
  {
    LOG("TODO incoming map packet");
    lob_free(packet);
  }

}

// send a map to this mote
void tmesh_map_send(link_t mote)
{
  /* TODO!
  ** send open first if not yet
  ** map packets have len 1 header which is our energy status, then binary frame body of:
  ** >0 energy, body up to 8 epochs (6 bytes) and rssi/status (1 byte) of each, max 56 body
  ** 0 energy, body up to 8 neighbors per packet, 5 bytes of hashname, 1 byte energy, and 1 byte of rssi/status each
  */
}

// new tmesh map channel
lob_t tmesh_on_open(link_t link, lob_t open)
{
  if(!link) return open;
  if(lob_get_cmp(open,"type","map")) return open;
  
  LOG("incoming tmesh map");

  // TODO get matching mote
  // create new channel for this block handler
//  mote->map = link_channel(link, open);
//  link_handle(link,mote->map,tmesh_map_handler,mote);
  
  

  return NULL;
}

tmesh_t tmesh_new(mesh_t mesh, lob_t options)
{
  tmesh_t tm;
  if(!mesh) return NULL;

  if(!(tm = malloc(sizeof (struct tmesh_struct)))) return LOG("OOM");
  memset(tm,0,sizeof (struct tmesh_struct));

  // connect us to this mesh
  tm->mesh = mesh;
  xht_set(mesh->index, "tmesh", tm);
  mesh_on_path(mesh, "tmesh", tmesh_on_path);
  mesh_on_open(mesh, "tmesh_open", tmesh_on_open);
  
  return tm;
}

void tmesh_free(tmesh_t tm)
{
  cmnty_t c, next;
  if(!tm) return;
  for(c=tm->coms;c;c=next)
  {
    next = c->next;
    cmnty_free(c);
  }
  lob_free(tm->pubim);
  free(tm);
  return;
}

tmesh_t tmesh_loop(tmesh_t tm)
{
  cmnty_t c;
  lob_t packet;
  mote_t m;
  if(!tm) return LOG("bad args");

  // process any packets into mesh_receive
  for(c = tm->coms;c;c = c->next) 
    for(m=c->motes;m;m=m->next)
      while((packet = util_chunks_receive(m->chunks)))
        mesh_receive(tm->mesh, packet, c->pipe); // TODO associate mote for neighborhood
  
  return tm;
}

// all devices
radio_t radio_devices[RADIOS_MAX] = {0};

// validate medium by checking energy
uint32_t medium_check(tmesh_t tm, uint8_t medium[5])
{
  int i;
  uint32_t energy;
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((energy = radio_devices[i]->energy(tm, medium))) return energy;
  }
  return 0;
}

// get the full medium
medium_t medium_get(tmesh_t tm, uint8_t medium[5])
{
  int i;
  medium_t m;
  // get the medium from a device
  for(i=0;i<RADIOS_MAX && radio_devices[i];i++)
  {
    if((m = radio_devices[i]->get(tm,medium))) return m;
  }
  return NULL;
}

uint8_t medium_public(medium_t m)
{
  if(m && m->bin[0] > 128) return 0;
  return 1;
}


radio_t radio_device(radio_t device)
{
  int i;
  for(i=0;i<RADIOS_MAX;i++)
  {
    if(radio_devices[i]) continue;
    radio_devices[i] = device;
    device->id = i;
    return device;
  }
  return NULL;
}

tmesh_t tmesh_knock(tmesh_t tm, knock_t k, uint64_t from, radio_t device)
{
  cmnty_t com;
  mote_t mote;
  struct knock_struct ktmp = {0,0,0,0,0,0,0};
  if(!tm || !k) return LOG("bad args");

  for(com=tm->coms;com;com=com->next)
  {
    if(device && com->medium->radio != device->id) continue;
    // TODO check c->sync ping/echo
    ktmp.com = com;
    // check every mote
    for(mote=com->motes;mote;mote=mote->next)
    {
      if(!mote_knock(mote,&ktmp,from)) continue;
      // TODO compare ktmp and k
      memcpy(k, &ktmp, sizeof(struct knock_struct));
      // TX > RX
      // LINK > PAIR > ECHO > PING
      /*
      if(mote->knock->dir == TX)
      {
        if(!tm->tx || tm->tx->knock->type > mote->knock->type || tm->tx->kstart < mote->kstart) tm->tx = mote;
      }else{
        if(!tm->rx || tm->rx->knock->type > mote->knock->type || tm->rx->kstart < mote->kstart) tm->rx = mote;
      }
      */
      
    }
  }
  
  // make sure a knock was found
  if(!k->com) return NULL;

  return tm;
}


tmesh_t tmesh_knocking(tmesh_t tm, knock_t k, uint8_t *frame)
{
  uint8_t nonce[8];
  uint32_t win;
  if(!tm || !k || !frame) return LOG("bad args");
  
  win = util_sys_long(k->win);
  memset(nonce,0,8);
  memcpy(nonce,&win,4); // nonce area

  // TODO copy in tx data from util_chunks

  // ciphertext frame
  memset(frame,0,64);
  chacha20(k->epoch->secret,nonce,frame,64);

  return tm;
}

// signal once a knock has been sent/received for this mote
tmesh_t tmesh_knocked(tmesh_t tm, knock_t k, uint8_t *frame)
{
  if(!tm || !k || !frame) return LOG("bad args");
  
  // TODO, need to handle if it wasn't performed?

  switch(k->epoch->type)
  {
    case PING:
      // if RX generate echo TX
      break;
    case ECHO:
      // if RX then create pair
      break;
    case PAIR:
      break;
    case LINK:
      break;
    default :
      break;
  }
  
  if(k->epoch->dir == TX)
  {
    // TODO check priv coms match device and ping->kchan
    // set up echo RX
  }
  
  return tm;
}

mote_t mote_new(link_t link)
{
  mote_t m;
  
  if(!(m = malloc(sizeof(struct mote_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct mote_struct));
  
  if(!(m->chunks = util_chunks_new(64))) return mote_free(m);
  m->link = link;

  return m;
}

mote_t mote_free(mote_t m)
{
  if(!m) return NULL;
  util_chunks_free(m->chunks);
  free(m);
  return NULL;
}

// set best knock win/chan/start/stop
mote_t mote_knock(mote_t m, knock_t k, uint64_t from)
{
  uint8_t pad[8];
  uint8_t nonce[8];
  uint64_t offset, start, stop;
  uint32_t win, lwin;
  epoch_t e;
  medium_t medium;
  if(!m || !k || !k->com) return LOG("bad args");

  k->epoch = NULL;
  k->mote = m;
  medium = k->com->medium;

  // get the best one
  for(e=m->epochs;e;e=e->next)
  {
    // normalize nonce
    memset(nonce,0,8);
    switch(e->type)
    {
      case PING:
        win = 0;
        e->base = from; // is always now
        break;
      case ECHO:
        win = 1;
        break;
      case PAIR:
      case LINK:
        win = ((from - e->base) / EPOCH_WINDOW);
        break;
      default :
        continue;
    }
    if(!e->base) continue;

    lwin = util_sys_long(win);
    memset(nonce,0,8);
    memcpy(nonce,&lwin,4);
  
    // ciphertext the pad
    memset(pad,0,8);
    chacha20(e->secret,nonce,pad,8);
    
    offset = util_sys_long((unsigned long)(pad[2])) % (EPOCH_WINDOW - medium->min);
    start = e->base + (EPOCH_WINDOW*win) + offset;
    stop = start + medium->min;
    if(!k->epoch || stop < k->stop)
    {
      // rx never trumps tx
      if(k->epoch && k->epoch->dir == TX && e->dir == RX) continue;
      k->epoch = e;
      k->win = win;
      k->chan = util_sys_short((unsigned short)(pad[0])) % medium->chans;
      k->start = start;
      k->stop = stop;
    }
  }
  
  return m;
}
