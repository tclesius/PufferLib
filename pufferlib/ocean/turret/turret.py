import numpy as np
import gymnasium
import pufferlib
from pufferlib.ocean.turret import binding


class Turret(pufferlib.PufferEnv):
    def __init__(self, num_envs=1, render_mode=None, buf=None, seed=0):
        self.single_observation_space = gymnasium.spaces.Box(
            low=-2.0, high=2.0, shape=(12,), dtype=np.float32
        )
        self.single_action_space = gymnasium.spaces.MultiDiscrete([2, 2, 2])

        self.render_mode = render_mode
        self.num_agents = num_envs

        super().__init__(buf)
        self.c_envs = binding.vec_init(
            self.observations,
            self.actions,
            self.rewards,
            self.terminals,
            self.truncations,
            num_envs,
            seed,
        )

    def reset(self, seed=0):
        binding.vec_reset(self.c_envs, seed)
        return self.observations, []

    def step(self, actions):
        self.actions[:] = actions
        binding.vec_step(self.c_envs)
        info = [binding.vec_log(self.c_envs)]
        return (self.observations, self.rewards, self.terminals, self.truncations, info)

    def render(self):
        binding.vec_render(self.c_envs, 0)

    def close(self):
        binding.vec_close(self.c_envs)


if __name__ == "__main__":
    env = Turret(num_envs=1, render_mode="human")
    obs, _ = env.reset()

    import time

    while True:
        actions = np.random.randint(0, 2, (env.num_agents, 3))
        obs, rewards, terminals, truncations, info = env.step(actions)
        env.render()
        time.sleep(1 / 60)

        if terminals[0]:
            obs, _ = env.reset()

    env.close()
