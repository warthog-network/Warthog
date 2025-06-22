#include "all.hpp"
#include "block/chain/history/history.hpp"

namespace api {
void Block::set_reward(Reward r)
{
    if (actions.reward.has_value())
        throw std::runtime_error("Database error, each block can only have one reward transaction");
    actions.reward = r;
}

}
