/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-server.
 *
 * libbitcoin-server is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bitcoin/server/server_node.hpp>

#include <future>
#include <iostream>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/server/config/config.hpp>
#include <bitcoin/server/config/settings.hpp>
#include <bitcoin/server/message.hpp>
#include <bitcoin/server/service/fetch_x.hpp>
#include <bitcoin/server/service/util.hpp>

namespace libbitcoin {
namespace server {

using namespace bc::chain;
using namespace bc::node;
using std::placeholders::_1;
using std::placeholders::_2;

server_node::server_node(const settings_type& config)
  : full_node(config),
    retry_start_timer_(memory_threads_.service())
{
}

bool server_node::start(const settings_type& config)
{
    return full_node::start(config);
}

void server_node::subscribe_blocks(block_notify_callback notify_block)
{
    block_sunscriptions_.push_back(notify_block);
}

void server_node::subscribe_transactions(transaction_notify_callback notify_tx)
{
    tx_subscriptions_.push_back(notify_tx);
}

void server_node::new_unconfirm_valid_tx(const std::error_code& ec,
    const index_list& unconfirmed, const transaction_type& tx)
{
    full_node::new_unconfirm_valid_tx(ec, unconfirmed, tx);

    if (ec == bc::error::service_stopped)
        return;

    // Fire server protocol tx subscription notifications.
    for (const auto notify: tx_subscriptions_)
        notify(tx);
}

void server_node::broadcast_new_blocks(const std::error_code& ec,
    uint32_t fork_point, const blockchain::block_list& new_blocks,
    const blockchain::block_list& replaced_blocks)
{
    broadcast_new_blocks(ec, fork_point, new_blocks, replaced_blocks);

    if (ec == bc::error::service_stopped)
        return;

    if (fork_point < BN_CHECKPOINT_HEIGHT)
        return;

    // Fire server protocol block subscription notifications.
    for (auto new_block: new_blocks)
    {
        const size_t height = ++fork_point;
        for (const auto notify: block_sunscriptions_)
            notify(height, *new_block);
    }
}

void server_node::fullnode_fetch_history(server_node& node,
    const incoming_message& request, queue_send_callback queue_send)
{
    uint32_t from_height;
    payment_address address;
    if (!unwrap_fetch_history_args(address, from_height, request))
        return;

    const auto handler = 
        std::bind(send_history_result,
            _1, _2, request, queue_send);

    fetch_history(node.blockchain(), node.transaction_indexer(), address,
        handler, from_height);
}

} // namespace server
} // namespace libbitcoin
