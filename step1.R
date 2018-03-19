# Read Raw Data
rawData <- readLines("../../log1.txt")
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
df <- data.frame(index = numeric(length(dataData)),
                 sending = numeric(length(dataData)),
                 recieving = numeric(length(dataData)),
                 rtt = numeric(length(dataData)),
                 hopcount = numeric(length(dataData)),
                 stringsAsFactors = FALSE)

for (i in 1:length(dataData)) {
  item <- strsplit(as.character(dataData[i]), " ")
  index <- as.numeric(item[[1]][2])
  df$index[i] <- index
  recieveTime <- as.numeric(item[[1]][1])
  for (j in length(interestData):1) {
    item2 <- strsplit(as.character(interestData[j]), " ")
    index2 <- as.numeric(item2[[1]][2])
    sendTime <- as.numeric(item2[[1]][1])
    if (index2 - index == 0) {
      df$sending[i] <- sendTime
      df$recieving[i] <- recieveTime
      df$rtt[i] <- (recieveTime*1000000000 - sendTime*1000000000)# - 214296000
      break
    }
  }
  for (j in length(RecoveryInterestData):1) {
    item2 <- strsplit(as.character(RecoveryInterestData[j]), " ")
    index2 <- as.numeric(item2[[1]][2])
    sendTime <- as.numeric(item2[[1]][1])
    if (index2 - index == 0) {
      df$sending[i] <- sendTime
      df$recieving[i] <- recieveTime
      df$rtt[i] <- (recieveTime*1000000000 - sendTime*1000000000)# - 214296000
      break
    }
  }
  item3 <- strsplit(as.character(hopData[i]), " ")
  df$hopcount[i] <- as.numeric(item3[[1]][2])
}

# show plot
plot(df$rtt[1:350], xlab="Data ID", ylab="Round-Trip Time")
plot(df$hopcount[1:350], xlab="Data ID", ylab="Hop Number")
rttsum <- summary(df$rtt[1:100])

# packets received per sencod
df2 <- data.frame(second = numeric(50),
                  packetNum = numeric(50),
                  stringsAsFactors = FALSE)
for (i in 1:50) {
  df2$second[i] <- i
  counter <- 0
  for (j in 1:length(dataData)) {
    item <- strsplit(as.character(dataData[j]), " ")
    recieveTime <- as.numeric(item[[1]][1])
    if (recieveTime <= i && recieveTime > i - 1) {
      counter <- counter + 1
    }
    if (recieveTime > i) {
      break
    }
  }
  df2$packetNum[i] <- counter
}
plot(df2$packetNum[1:50], xlab="Time", ylab="Downloading Rate", type="o", col="blue")


# calculate how long it take to recovery all the pre-fetched Data packets
deltaTime <- function(startIndex, endIndex) {
sendingTime <- 0
for (i in 1:length(RecoveryInterestData)) {
  item <- strsplit(as.character(RecoveryInterestData[i]), " ")
  index <- as.numeric(item[[1]][2])
  sendTime <- as.numeric(item[[1]][1])
  if (index >= startIndex && index <= endIndex) {
    sendingTime <- sendTime
    break
  }
}
latestRecievingTime <- 0
for (i in 1:length(dataData)) {
  item <- strsplit(as.character(dataData[i]), " ")
  index <- as.numeric(item[[1]][2])
  recieveTime <- as.numeric(item[[1]][1])
  if (index >= startIndex && index <= endIndex) {
    if (recieveTime > latestRecievingTime) {
      latestRecievingTime = recieveTime
    }
  }
}
return(latestRecievingTime - sendingTime)
}
deltaTimeResult <- c(
deltaTime(startIndex = 55, endIndex = 63),
deltaTime(startIndex = 119, endIndex = 128),
deltaTime(startIndex = 184, endIndex = 193),
deltaTime(startIndex = 250, endIndex = 259),
deltaTime(startIndex = 315, endIndex = 324)
)
summary(deltaTimeResult)

