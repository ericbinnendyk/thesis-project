from keras.models import Sequential
from keras.layers import Dense
from keras.models import model_from_json

# Initialize a neural network with random weights

model = Sequential()
model.add(Dense(8, input_dim=6, activation='relu'))
model.add(Dense(8, activation='relu'))
model.add(Dense(2, activation='sigmoid')) # 2 output values for change in high and low priority token rates

# save model as JSON
f = open("model.json", "w")
f.write(model.to_json())
f.close()

# save weights as HDF5
model.save_weights("model.h5")