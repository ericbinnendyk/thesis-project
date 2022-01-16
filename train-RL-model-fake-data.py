from random import randrange

import tensorflow as tf
import numpy as np
from tensorflow import keras
from keras.layers import Dense

# This code was modified from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
class DQN(keras.Model):
  """Dense neural network class."""
  def __init__(self):
    super(DQN, self).__init__()
    self.dense1 = Dense(8, input_dim=(6,), activation="relu") # Initialize with random weights I guess
    self.dense2 = Dense(8, activation="relu")
    self.dense3 = Dense(num_actions, dtype=tf.float32) # No activation
    # How does this last one work? Does it give an array of num_actions outputs and whichever is the largest that's the action we'll take?
    # Update: Yes

  def call(self, x):
    """Forward pass."""
    x = self.dense1(x)
    x = self.dense2(x)
    return self.dense3(x)

def generate_data(array_of_six):
    for j in range(6):
        if j in {0, 1}:
            array_of_six[0,j] = randrange(0, 10)
        if j == 2:
            array_of_six[0,j] = randrange(0, 300)
        if j == 3:
            array_of_six[0,j] = randrange(0, 150)
        if j == 4:
            array_of_six[0,j] = randrange(0, 2500)
        if j == 5:
            array_of_six[0,j] = randrange(0, 1000)

def get_state():
    state = np.zeros((1,6))
    generate_data(state)
    return state

reward = randrange(0, 100)

def get_reward():
    new_reward = reward + randrange(-5, 5)
    if new_reward < 0 or new_reward > 100:
        return reward
    else:
        return new_reward

# Gets action represented as a single index from the array of possible actions.
def get_epsilon_action_thingy(state, epsilon):
    """Take random action with probability epsilon, else take best action."""
    # This was inspired by the code from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
    # The value epsilon sets the exploration vs exploitation level.
    result = tf.random.uniform((1,))
    if result < epsilon:
        return randrange(0, 2) # Random action (left or right).
    else:
        return tf.argmax(main_nn.predict(state)[0]).numpy() # Greedy action for state.
        # Okay, what does this do? It looks like main_nn is outputing something whose 0th row is a list of rewards for each action.

def Q(state, action):
    return main_nn.predict(state)[0,action]

def send_action(action):
    pass

num_actions = 2 # increase or decrease the token bucket fill rate by 1

gamma = 0.8

main_nn = DQN()
target_nn = DQN()

optimizer = keras.optimizers.Adam(1e-4)
mse = keras.losses.MeanSquaredError()

state = get_state() # generates a fake state but has an interface as if the state is coming from a real environment
# xyzzy. This doesn't work because of some dimension error even though "state" is supposed to be a 6-cell long linear array. Why?
'''action_list = main_nn.call(state)
print(action_list)
action = argmax(action_list)'''

while True:
    action = get_epsilon_action_thingy(state, 0.3) # calls the neural network
    send_action(action)
    # xyzzy. Is 0.3 the best value? Should it decrease over time?

    # this next part is the part that is done after 1 second when the network has had time to gather more data for a reward
    # xyzzy
    # Maybe the change shouldn't be so small? If the change in parameters for each timestep is small, the learned rewards for each action will not be so different, and the agent won't know which way to move

    reward = get_reward() # generates a fake reward but has an interface as if the reward is real

    new_state = get_state()

    predicted_Q = Q(state, action)
    actual_Q = reward + gamma*max(Q(new_state, 0), Q(new_state, 1))
    loss = (actual_Q - predicted_Q)**2 # .:|:;

    # Now train model
    #main_nn.train(state)
    # xyzzy: this code is pretty much plagiarized from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
    # I should make it simpler and amke sure I understand what's going on.
    with tf.GradientTape() as tape:
        predicted_Q = main_nn(state)
        action_mask = tf.one_hot(action, num_actions)
        masked_qs = tf.reduce_sum(action_mask * predicted_Q, axis=-1) # this is element-wise, multiplication. and then sum the values (all but one of which is 0)
        loss = mse(tf.constant([actual_Q]), masked_qs)
    # xyzzy: I don't actually know what goes on here. Is it a back propagation?
    grads = tape.gradient(loss, main_nn.trainable_variables)
    optimizer.apply_gradients(zip(grads, main_nn.trainable_variables))
    print(loss)

    state = new_state
