#ifndef _HMC_RING_H_
#define _HMC_RING_H_

#include <vector>
#include "config.h"
#include "hmc_notify.h"
#include "hmc_macros.h"
#include "hmc_link.h"

class hmc_cube;
class hmc_quad;

#define HMC_JTL_ALL_LINKS         ( HMC_MAX_LINKS/HMC_NUM_QUADS + HMC_NUM_QUADS + HMC_NUM_VAULTS / HMC_NUM_QUADS )
#define HMC_JTL_EXT_LINK( x )     ( x )
#define HMC_JTL_RING_LINK( x )    ( HMC_MAX_LINKS/HMC_NUM_QUADS + (x) )
#define HMC_JTL_VAULT_LINK( x )   ( HMC_MAX_LINKS/HMC_NUM_QUADS + HMC_NUM_QUADS + (x) )

class hmc_ring : private hmc_notify_cl {
private:
  unsigned id;
  hmc_cube* cub;

  hmc_notify links_notify;
  std::vector<hmc_link*> links;

  unsigned decode_link_of_packet(char* packet);
  bool _set_link(unsigned notifyid, unsigned id, hmc_link *link);

  ALWAYS_INLINE unsigned routing(unsigned nextquad)
  {
    // since this is a ring, we can't cross from 0 to 3 or 1 to 2.
    // we will route first up then right
    /*
       [00]  <- ^= 0b1 -> [01]

       ^=0b10             ^=0b10

       [10]  <- ^= 0b1 -> [11]

       scheme routes first among x-axis, than y-axis
     */
    unsigned shift = (nextquad ^ this->id) & 0b01;
    return this->id ^ (0b10 >> shift);
  }

  bool notify_up(void);

public:
  hmc_ring(unsigned id, hmc_notify *notify, hmc_cube* cub);
  ~hmc_ring(void);

  bool set_link(unsigned linkId, hmc_link* link, enum hmc_link_type linkType)
  {
    switch(linkType) {
    case HMC_LINK_RING:
      return this->_set_link(HMC_JTL_RING_LINK(linkId), this->id, link);
    case HMC_LINK_VAULT:
      return this->_set_link(HMC_JTL_VAULT_LINK(linkId), this->id, link);
    case HMC_LINK_EXTERN:
      return this->_set_link(HMC_JTL_EXT_LINK(linkId), this->id, link);
    case HMC_LINK_UNDEFINED:
      break;
    }
    return false;
  }

  void clock(void);
};

#endif /* #ifndef _HMC_RING_H_ */
