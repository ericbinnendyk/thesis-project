from numpy import mean

f = open("app-delays-trace.txt", 'r')
l = list(f)
f.close()

split_l = [x.strip().split('\t') for x in l]
split_l = [x for x in split_l if x[4] == "FullDelay"]

high_priority = [x for x in split_l if x[1] in {'Src1', 'Src3'}]
high_priority_delays = [float(x[5]) for x in high_priority]
mean_high_priority_delays = mean(high_priority_delays)

low_priority = [x for x in split_l if x[1] in {'Src2', 'Src4'}]
low_priority_delays = [float(x[5]) for x in low_priority]
mean_low_priority_delays = mean(low_priority_delays)

print(mean_high_priority_delays, mean_low_priority_delays)
