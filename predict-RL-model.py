# optimizes the parameters of a network simulation to params with higher predicted efficiency

import numpy as np
from keras.models import Sequential
from keras.layers import Dense
from keras.models import model_from_json
import sys

# load model
json_file = open('model.json', 'r')
loaded_model = model_from_json(json_file.read())
json_file.close()
loaded_model.load_weights("model.h5")

f = open('input.txt', 'r')
l = f.readlines()[0]
f.close()

state = [int(x) for x in line.strip('\n').split(' ')]

# chooses an action for the agent based on the prediction
def predict_and_print(state_):
    pred = loaded_model.predict(np.array([state_]))
    print("Action taken for settings {}: {:.2f}, {:.2f}".format(params_, pred[0], pred[1]), file=sys.stderr)
    return pred

pred = predict_and_print(state)

# converts result of prediction to a concrete action changing token bucket parameters
actual_pred = [0,0]
for i, p in enumerate(pred):
    if p >= 0:
        actual_pred[i] = np.random.binomial(1, p)
    else:
        actual_pred[i] = -np.random.binomial(1, -p)

# write prediction to output to feed back to simulation
f = open('output.txt', 'w')
print("{}, {}".format(actual_pred[0], actual_pred[1]))
f.close()