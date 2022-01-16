from random import randrange

import tensorflow as tf
from tensorflow import keras
from keras.models import Sequential # not sure if we need this
from keras.layers import Dense
from keras.models import model_from_json

import numpy as np
from numpy import loadtxt, mean

import sys
import time
import zmq

# This code was modified from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
class DQN(keras.Model):
  """Dense neural network class."""
  def __init__(self):
    super(DQN, self).__init__()
    self.dense1 = Dense(8, input_dim=(6,), activation="sigmoid") # Initialize with random weights I guess
    self.dense2 = Dense(8, activation="sigmoid")
    self.dense3 = Dense(num_actions, dtype=tf.float32, activation="sigmoid") # Activation to move the output into [0, 1], because the reward is always in that range
    # How does this last one work? Does it give an array of num_actions outputs and whichever is the largest that's the action we'll take?
    # Update: Yes

  def call(self, x):
    """Forward pass."""
    x = self.dense1(x)
    x = self.dense2(x)
    return self.dense3(x)

# Input format:
'''
Node: N1
queue_size1 queue_size2 ...other stuff... num_packets1 num_packets2
Node: N2
queue_size1 queue_size2 ...other stuff... num_packets1 num_packets2
Node: N1
tokens_per_second_1 tokens_per_second_2
Node: N2
tokens_per_second_1 tokens_per_second_2
'''

# Input to neural net:
# [queue_size1 queue_size2 num_packets1 num_packets2 tokens_per_second1 tokens_per_second2]

'''filename = sys.argv[1]
node_num = int(sys.argv[2])
f = open(filename, 'r')
lines = f.readlines()
f.close()'''

def receive_string_and_send_reply():
    #  Wait for next request from client
    message = socket.recv() # message is a bit string, not a normal string
    print("Received message: {}".format(message.split(b'\0')[0].decode('ascii')))

    #  Send pointless reply back to client
    socket.send(b"Received")
    return message.split(b'\0')[0].decode('ascii')

def extract_state_data(node_num):
    mode = "get_nums"
    curr_node = ''
    curr_input = [0, 0, 0, 0, 0, 0]
    have_collected_params = [False] * 6
    outputs = []
    # changed for testing
    while not all(have_collected_params):
        line = receive_string_and_send_reply()
        if line.startswith('Node: '):
            curr_node = line.strip().strip('Node: ')
        else:
            if int(curr_node) == node_num:
                data_type = line.split(': ')[0]
                line = line.split(': ')[-1]
                new_params = [float(param) for param in line.strip().split(' ')]
                if data_type == 'Rates':
                    # we have tokens per second
                    curr_input[4:6] = new_params
                    have_collected_params[4:6] = [True, True]
                if data_type == 'Queue sizes':
                    # we have stats on queue sizes
                    curr_input[0:2] = new_params
                    have_collected_params[0:2] = [True, True]
                if data_type == 'Incoming packets':
                    # we have stats on number of packets
                    curr_input[2:4] = new_params
                    have_collected_params[2:4] = [True, True]
    inputs = [curr_input]
    print("Received state", inputs)
    return np.array(inputs)

def get_fake_reward():
    global reward
    new_reward = reward + randrange(-5, 5)
    if new_reward < 0 or new_reward > 100:
        return reward
    else:
        reward = new_reward
        return new_reward

# Gets action represented as a single index from the array of possible actions.
def get_epsilon_action_thingy(state, epsilon):
    """Take random action with probability epsilon, else take best action."""
    # This was inspired by the code from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
    # The value epsilon sets the exploration vs exploitation level.
    result = tf.random.uniform((1,))
    if result < epsilon:
        print("Returning a random action")
        return randrange(0, num_actions) # Random action (left or right).
    else:
        print("Returning the predicted best action")
        return tf.argmax(main_nn.predict(state)[0]).numpy() # Greedy action for state.
        # Okay, what does this do? It looks like main_nn is outputing something whose 0th row is a list of rewards for each action.

def Q(state, action):
    return main_nn.predict(state)[0,action]

# Send action to C++ simulation
def send_action(action):
    message = socket.recv()

    if message.split(b'\0')[0].decode('ascii') == 'Action':
        socket.send(bytes(str(action) + '\0', 'ascii'))
    else:
        raise RuntimeError("send_action called when receiver is not ready")

# We'll implement this later
'''def extract_reward_data()
    socket.send(b"Reward\0")
    message = socket.recv()
    message_str = message.split(b'\0')[0].decode('ascii')
    node, reward = message_str.split('\n')
    if int(node.strip().strip('Node: ')) == node_num:
        return float(reward)'''

# Dad writes:
# loop:
#   do one step of the simulation
#   send results and status to python AI
#   wait for reply from AI, which might include config changes

node_num = int(sys.argv[1])
context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:{}".format(node_num + 5550))
print("Connected to client", "tcp://*:{}".format(node_num + 5550))
num_actions = 6
gamma = 0.8 # future reward discount factor
epsilon = 0.95 # epsilon is the probability of taking a random choice rather than the best choice. It decreases from 0.95 down to 0.05 over time.
main_nn = DQN()
#reward = randrange(0, 100)
opt = keras.optimizers.Adam(learning_rate=0.1)
mse = keras.losses.MeanSquaredError()

main_nn.compile(optimizer=opt, loss='mse')

aoeu = receive_string_and_send_reply()
print("First \"reward\" is", aoeu) # first reward should be uhhh, real?
# I guess we can just ignore the first reward because we can't measure the state that led to it

# I believe this happens after one second, when the first state gets sent.
curr_state = extract_state_data(node_num)
while True:
    action = get_epsilon_action_thingy(curr_state, epsilon) # calls the neural network
    print("Action is", action)
    # update the epsilon value so the chance of taking a random action decreases
    if epsilon > 0.06: epsilon -= 0.05
    send_action(action)
    # xyzzy. Is 0.3 the best value? Should it decrease over time?

    # this next part is the part that is done after 1 second when the network has had time to gather more data for a reward
    # xyzzy
    # Maybe the change in parameters for each action shouldn't be so small? If the change in parameters for each timestep is small, the learned rewards for each action will not be so different, and the agent won't know which way to move

    reward = float(receive_string_and_send_reply()) # the reward is supposed to be fetched up here, but currently the only way to get it is via the next state.

    new_state = extract_state_data(node_num) # for use later

    '''predicted_Q = Q(curr_state, action)
    actual_Q = reward + gamma*max(Q(new_state, 0), Q(new_state, 1))
    loss = (actual_Q - predicted_Q)**2 # .:|:;'''

    '''# Now train model
    #main_nn.train(state)
    # xyzzy: this code is pretty much plagiarized from https://markelsanz14.medium.com/introduction-to-reinforcement-learning-part-3-q-learning-with-neural-networks-algorithm-dqn-1e22ee928ecd
    # I should make it simpler and amke sure I understand what's going on.
    with tf.GradientTape() as tape:
        predicted_Q = main_nn(curr_state)
        action_mask = tf.one_hot(action, num_actions)
        masked_qs = tf.reduce_sum(action_mask * predicted_Q, axis=-1) # this is element-wise, multiplication. and then sum the values (all but one of which is 0)
        loss = mse(tf.constant([actual_Q]), masked_qs)
    # xyzzy: I don't actually know what goes on here. Is it a back propagation?
    grads = tape.gradient(loss, main_nn.trainable_variables)
    optimizer.apply_gradients(zip(grads, main_nn.trainable_variables))
    print(loss)'''

    # Train the neural network
    # calculate the loss function and the gradient value. This code is inspired by here: https://keras.io/api/optimizers/#adam
    # but looks pretty similar to the above. I'm still not sure how GradientTape works or if I'm using it correctly.
    actual_Q = reward + gamma*max(Q(new_state, 0), Q(new_state, 1))
    with tf.GradientTape() as tape:
        # pass current state into neural network but as a variable somehow so gradient can be calculated
        logits = main_nn(curr_state)
        # I don't know how deep Q-learning is "supposed" to be trained when we get a correction on only one of the rewards.
        # imagine the predicted rewards are [a, b], the chosen action is 0, and the reward (plus future reward) for 0 is a'.
        # action_mask = [1, 0]
        action_mask = tf.one_hot(action, num_actions)
        predicted_Q = logits[0,action]
        print("Actual reward:", action_mask * actual_Q)
        print("Predicted reward:", action_mask * predicted_Q)
        '''# action_mask * predicted_Q = [a, 0]
        masked_qs = tf.reduce_sum(action_mask * predicted_Q, axis=-1)
        print(masked_qs)
        # masked_qs = [a]'''
        loss = mse(action_mask * predicted_Q, action_mask * actual_Q)
        print("Loss:", loss)
        # actual_Q = a'
        # Okay, by the time we got to here, _hopefully_ loss is calculated as a function of the weights, so the weights can be adjusted.
    grads = tape.gradient(loss, main_nn.trainable_variables)
    # update model weights
    opt.apply_gradients(zip(grads, main_nn.trainable_variables))
    print("Updated neural network")
    # get next state and start again
    curr_state = new_state

print("x and y are")
print(X, y)

# neural network model!
# load model from JSON
json_file = open('model.json', 'r')
model = model_from_json(json_file.read())
json_file.close()
model.load_weights("model.h5")

# compile the keras model
model.compile(loss='binary_crossentropy', optimizer='adam', metrics=['accuracy'])

print(X)

model.fit(X, y)

# evaluate the model
loss, accuracy = model.evaluate(X, y)
print("Accuracy: {}".format(accuracy))

# save model as JSON
f = open("model.json", "w")
f.write(model.to_json())
f.close()

# save weights as HDF5
model.save_weights("model.h5")