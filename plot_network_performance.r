# Calculate metrics of network performance, from the trace files produced by ndn-qos-2-router.cpp

setwd("~/NDN_QoS_4/ndnQoS/ns-3")

# Plot the packet delay
data = read.table("app-delays-trace.txt", header=T)
high_priority_delays = data[(data$Node == 'Src1' | data$Node == 'Src3') & data$Type == 'FullDelay','DelayS']
mean_high_priority_delays = mean(high_priority_delays)

low_priority_delays = data[(data$Node == 'Src2' | data$Node == 'Src4') & data$Type == 'FullDelay','DelayS']
mean_low_priority_delays = mean(low_priority_delays)

barplot(c(mean_high_priority_delays, mean_low_priority_delays), ylab="delay (seconds)", names=c("High priority delays", "Low priority delays"))

# Plot the total packet drop for the base ns-3 queue
data = read.table("drop-trace.txt", header=T)

drops_per_time = c()
for (x in seq(1,max(data$Time),1)) {
  drops_per_time = c(drops_per_time, sum(data$Packets[data$Time == x]))
}

plot(c(0,50), c(0,50), type="n", xlab="Time (seconds)", ylab="Number of dropped packets per sec")
points(x=seq(1, max(data$Time), 1), y=drops_per_time)

# Find the packet trace
data = read.table("rate-trace.txt", header=T)

total_out_interests = sum(data$Kilobytes[startsWith(unlist(lapply(data$Node, toString)), 'Src') & startsWith(unlist(lapply(data$FaceDescr, toString)), 'netdev') & data$Type == 'OutInterests'])
total_out_data = sum(data$Kilobytes[startsWith(unlist(lapply(data$Node, toString)), 'Dst') & startsWith(unlist(lapply(data$FaceDescr, toString)), 'netdev') & data$Type == 'OutData'])
total_traffic = total_out_interests + total_out_data
print(total_traffic)

# Plot the total packet drop for the other queues
data = read.table("priority-queue-drop-trace.txt", header=T)

drop1_per_time = c()
for (x in seq(1,max(data$Time),1)) {
  drop1_per_time = c(drop1_per_time, sum(data$Drop1[data$Time == x]))
}

drop2_per_time = c()
for (x in seq(1,max(data$Time),1)) {
  drop2_per_time = c(drop2_per_time, sum(data$Drop2[data$Time == x]))
}

plot(c(0,50), c(0,200), type="n", xlab="Time (seconds)", ylab="Number of dropped packets per sec")
points(x=seq(1, max(data$Time), 1), y=drop1_per_time, col="red")
points(x=seq(1, max(data$Time), 1), y=drop2_per_time, col="blue")
