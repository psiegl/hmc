#include "hmc_link.h"

hmc_link::hmc_link(uint64_t *i_cur_cycle) :
  hmc_notify_cl(),
  not_rx_q(-1, nullptr, this),
  rx_q(i_cur_cycle, &rx_buf, &not_rx_q),
  not_rx_buf(-1, nullptr, this),
  rx_buf(&not_rx_buf),
  tx(nullptr),
  binding(nullptr)
{
}

hmc_link::~hmc_link(void)
{
}

hmc_link_queue* hmc_link::__get_rx_q(void)
{
  return &this->rx_q;
}

void hmc_link::set_ilink_notify(unsigned id, hmc_notify *notify)
{
  this->not_rx_q.set(id, notify);
  this->not_rx_buf.set(id, notify);

  this->rx_q.set_id(id);
}


void hmc_link::re_adjust_links(unsigned link_bitwidth, float link_bitrate)
{
  this->__get_rx_q()->re_adjust(link_bitwidth, link_bitrate);
  this->binding->__get_rx_q()->re_adjust(link_bitwidth, link_bitrate);

  this->re_adjust_size(128 * 17);
  this->binding->re_adjust_size(128 * 17);
}

void hmc_link::re_adjust_size(unsigned buf_bitsize)
{
  this->rx_buf.adjust_size(buf_bitsize);
}

void hmc_link::connect_linkports(hmc_link *part)
{
  this->set_binding(part);
  part->set_binding(this);
}

void hmc_link::set_binding(hmc_link *part)
{
  this->tx = part->__get_rx_q();
  this->binding = part;
}


void hmc_link::clock(void)
{
  if (this->not_rx_q.get_notification())
    this->rx_q.clock();
// ToDo -> will be most likely a combination out of front and pop_front
}

bool hmc_link::notify_up(void)
{
  return (!this->not_rx_q.get_notification()
          && !this->not_rx_buf.get_notification());
}
