#include <iostream>
#include <cstring>
#include "src/hmc_notify.h"
#include "src/hmc_ring.h"
#include "src/hmc_link.h"
#include "src/hmc_cube.h"
#include "src/hmc_sim.h"


int main(int argc, char* argv[])
{
  unsigned cubes = 2;
  hmc_sim sim(cubes,2,4,8, HMCSIM_FULL_LINK_WIDTH, HMCSIM_FULL_LINK_WIDTH);
  hmc_link* slid = sim.hmc_link_to_slid(0, 0, 0, HMCSIM_FULL_LINK_WIDTH);
  hmc_notify slidnotify;
  slid->set_ilink_notify(0, &slidnotify);

  sim.hmc_link_config(0, 1, 1, 0, HMCSIM_FULL_LINK_WIDTH);
  sim.hmc_link_config(0, 3, 1, 2, HMCSIM_FULL_LINK_WIDTH);

  unsigned sendpacketleninbit = 2*FLIT_WIDTH;

  unsigned issue = 100;
  unsigned send_ctr = 0;
  unsigned recv_ctr = 0;

  unsigned clks = 0;
  unsigned *track = new unsigned[issue];
  do
  {
    if(issue > send_ctr && slid->get_olink()->has_space(sendpacketleninbit))
    {
      uint64_t *packet = new uint64_t[(sendpacketleninbit/FLIT_WIDTH) << 1];
      memset(packet, 0, (sendpacketleninbit / FLIT_WIDTH << 1) * sizeof(uint64_t));
      packet[0] = 0;
      packet[0] |= ((0x33) & 0x7F); // RD64
      packet[0] |= (((sendpacketleninbit/FLIT_WIDTH) & 0x1F) << 7);

      unsigned addr = 0b110000000000; // quad 3
      unsigned cube = 0;
      packet[0] |= (((uint64_t)((addr) & 0x3FFFFFFFFull)) << 24);
      packet[0] |= (uint64_t)(cube) << 61;

      //std::cout << "header in main: " << packet[0] << std::endl;

      slid->get_olink()->push_back( packet, sendpacketleninbit );
      track[send_ctr] = clks;
      send_ctr++;
    }
    if(slidnotify.get_notification()) {
      unsigned recvpacketleninbit;
      uint64_t* packet = (uint64_t*)slid->get_ilink()->front(&recvpacketleninbit);
      if(packet != nullptr)
      {
        slid->get_ilink()->pop_front();
        track[recv_ctr] = clks - track[recv_ctr];
        recv_ctr++;
        if(recv_ctr >= issue)
          break;
      }
    }
    // set clk anyway
    clks++;
    //if(clks > 311)
    //  exit(0);
    sim.clock();
  } while(true);

  std::cout << "clks: " << clks << std::endl;
  for(unsigned i=0; i<issue; i++) {
    std::cout << "pkt " << i << ". -> clks " << track[i] << std::endl;
  }

  delete[] track;
  std::cout << "done in " << clks << " clks " << std::endl;

  return 0;
}
