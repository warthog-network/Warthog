#include "all.hpp"

namespace api {
void Block::set_reward(block::Reward r)
{
    if (actions.reward.has_value())
        throw std::runtime_error("Database error, each block can only have one reward transaction");
    actions.reward = r;
}

}
