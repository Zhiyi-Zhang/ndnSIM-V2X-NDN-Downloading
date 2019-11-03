# Read Raw Data
rawData <- readLines("real-time.txt")
options(scipen=999)

# Extract Data Data
dataPattern <- "^\\+([.0-9]*).* < DATA for ([0-9]*)$"
dataData <- regmatches(rawData, gregexpr(dataPattern, rawData))
dataData <- dataData[grep(dataPattern, dataData)]
for (i in 1:length(dataData)) {
  dataData[i] <- sub(dataPattern, "\\1 \\2", dataData[i])
}

# Extract Interest Data
interestPattern <- "^\\+([.0-9]*).*Interest for ([0-9]*)$"
interestData <- regmatches(rawData, gregexpr(interestPattern, rawData))
interestData <- interestData[grep(interestPattern, interestData)]
for (i in 1:length(interestData)) {
  interestData[i] <- sub(interestPattern, "\\1 \\2", interestData[i])
}

# Extract Prefetch Interest Data
PreFetchInterestPattern <- "^\\+([.0-9]*).* > Pre-Fetch Interest for ([0-9]*)$"
PreFetchInterestData <- regmatches(rawData, gregexpr(PreFetchInterestPattern, rawData))
PreFetchInterestData <-PreFetchInterestData[grep(PreFetchInterestPattern, PreFetchInterestData)]
for (i in 1:length(PreFetchInterestData)) {
  PreFetchInterestData[i] <- sub(PreFetchInterestPattern, "\\1 \\2", PreFetchInterestData[i])
}
# duplicatedInterestNumber <- length(PreFetchInterestData)
# duplicatedInterestRatio <- duplicatedInterestNumber/(length(interestData) + duplicatedInterestNumber)

# Extract Recovery Interest Data
RecoveryInterestPattern <- "^\\+([.0-9]*).* > Recovery Interest for ([0-9]*)$"
RecoveryInterestData <- regmatches(rawData, gregexpr(RecoveryInterestPattern, rawData))
RecoveryInterestData <-RecoveryInterestData[grep(RecoveryInterestPattern, RecoveryInterestData)]
for (i in 1:length(RecoveryInterestData)) {
  RecoveryInterestData[i] <- sub(RecoveryInterestPattern, "\\1 \\2", RecoveryInterestData[i])
}

# Extract Dropped Data Data
DroppedPreFetchDataPattern <- "^\\+([.0-9]*).*< Pre.Fetch DATA..([0-9]*)..DROP.$"
DroppedPreFetchDataData <- regmatches(rawData, gregexpr(DroppedPreFetchDataPattern, rawData))
DroppedPreFetchDataData <-DroppedPreFetchDataData[grep(DroppedPreFetchDataPattern, DroppedPreFetchDataData)]
DroppedPreFetchDataNumber <- length(DroppedPreFetchDataData)
DroppedPreFetchDataRatio <- DroppedPreFetchDataNumber/(length(dataData) + DroppedPreFetchDataNumber)

# Extract Hop Data
hopcountPattern <- "^\\+([.0-9]*).*Hop count: ([0-9]*)$"
hopData <- regmatches(rawData, gregexpr(hopcountPattern, rawData))
hopData <- hopData[grep(hopcountPattern, hopData)]
for (i in 1:length(hopData)) {
  hopData[i] <- sub(hopcountPattern, "\\1 \\2", hopData[i])
}

# Calculate RTT
# df <- data.frame(index = numeric(length(dataData)),
#                  sending = numeric(length(dataData)),
#                  recieving = numeric(length(dataData)),
#                  rtt = numeric(length(dataData)),
#                  hopcount = numeric(length(dataData)),
#                  stringsAsFactors = FALSE)
# 
# for (i in 1:length(dataData)) {
#   item <- strsplit(as.character(dataData[i]), " ")
#   index <- as.numeric(item[[1]][2])
#   df$index[i] <- index
#   recieveTime <- as.numeric(item[[1]][1])
#   for (j in length(interestData):1) {
#     item2 <- strsplit(as.character(interestData[j]), " ")
#     index2 <- as.numeric(item2[[1]][2])
#     sendTime <- as.numeric(item2[[1]][1])
#     if (index2 - index == 0) {
#       df$sending[i] <- sendTime
#       df$recieving[i] <- recieveTime
#       df$rtt[i] <- (recieveTime*1000000000 - sendTime*1000000000)# - 214296000
#       break
#     }
#   }
#   for (j in length(RecoveryInterestData):1) {
#     item2 <- strsplit(as.character(RecoveryInterestData[j]), " ")
#     index2 <- as.numeric(item2[[1]][2])
#     sendTime <- as.numeric(item2[[1]][1])
#     if (index2 - index == 0) {
#       df$sending[i] <- sendTime
#       df$recieving[i] <- recieveTime
#       df$rtt[i] <- (recieveTime*1000000000 - sendTime*1000000000)# - 214296000
#       break
#     }
#   }
#   item3 <- strsplit(as.character(hopData[i]), " ")
#   df$hopcount[i] <- as.numeric(item3[[1]][2])
# }
# 
# # show plot
# #plot(df$rtt[1:nrow(df)], xlab="Data ID", ylab="Round-Trip Time")
# pdf("interest-only-hop-count.pdf",width=7,height=4) 
# par(mar=c(4,4,0,0)+0.1)
# plot(df$hopcount[1:nrow(df)], 
#      xlab="Target Data ID", ylab="Hop Number", 
#      ylim = c(1, 5),
#      pch = 1, cex.lab=1.5, cex.axis=1.5, cex.main=1.5, cex.sub=1.5)
# dev.off()
# rttsum <- summary(df$rtt[1:100])

# packets received per sencod
scale <- 0.5
table_row <- 60/scale
df2 <- data.frame(second = numeric(table_row),
                  packetNum = numeric(table_row),
                  stringsAsFactors = FALSE)
for (i in 1:table_row) {
  df2$second[i] <- i*scale
  counter <- 0
  for (j in 1:length(dataData)) {
    item <- strsplit(as.character(dataData[j]), " ")
    recieveTime <- as.numeric(item[[1]][1])
    if (recieveTime <= i*scale && recieveTime > (i - 1)*scale ) {
      counter <- counter + 1
    }
  }
  df2$packetNum[i] <- counter
}
pdf("full.pdf",,width=6,height=3.5)
par(mar=c(2,2,0,0)+0.2)
plot(df2$second[1:table_row], df2$packetNum[1:table_row]*(1/scale), 
     xlab="Simulation Time (s)", ylab="Downloading (pkts/s)",
     ylim = c(0, 40),
     type="l", col="blue", cex.lab=1.5, cex.axis=1.5, cex.main=1.5, cex.sub=1.5)
dev.off()