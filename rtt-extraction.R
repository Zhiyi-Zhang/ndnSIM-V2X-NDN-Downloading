# Read Raw Data
rawData <- readLines("../../log2.txt")

# Extract Data Data
dataPattern <- "^\\+([.0-9]*).* < DATA for ([0-9]*)$"
dataData <- regmatches(rawData, gregexpr(dataPattern, rawData))
dataData <- dataData[grep(dataPattern, dataData)]
for (i in 1:length(dataData)) {
  dataData[i] <- sub(dataPattern, "\\1 \\2", dataData[i])
}

# Extract Interest Data
interestPattern <- "^\\+([.0-9]*).* > Interest for ([0-9]*)$"
interestData <- regmatches(rawData, gregexpr(interestPattern, rawData))
interestData <- interestData[grep(interestPattern, interestData)]
for (i in 1:length(interestData)) {
  interestData[i] <- sub(interestPattern, "\\1 \\2", interestData[i])
}

# Extract Prefetch Interest Data
PreFetchInterestPattern <- "^\\+([.0-9]*).* > Pre-Fetch Interest.*([0-9]*)$"
PreFetchInterestData <- regmatches(rawData, gregexpr(PreFetchInterestPattern, rawData))
PreFetchInterestData <-PreFetchInterestData[grep(PreFetchInterestPattern, PreFetchInterestData)]
duplicatedInterestNumber <- length(PreFetchInterestData)
duplicatedInterestRatio <- duplicatedInterestNumber/(length(interestData) + duplicatedInterestNumber)

# Extract Dropped Data Data
DroppedPreFetchDataPattern <- "^\\+([.0-9]*).*< Pre.Fetch DATA..([0-9]*)..DROP.$"
DroppedPreFetchDataData <- regmatches(rawData, gregexpr(DroppedPreFetchDataPattern, rawData))
DroppedPreFetchDataData <-DroppedPreFetchDataData[grep(DroppedPreFetchDataPattern, DroppedPreFetchDataData)]
DroppedPreFetchDataNumber <- length(DroppedPreFetchDataData)
DroppedPreFetchDataRatio <- DroppedPreFetchDataNumber/(length(dataData) + DroppedPreFetchDataNumber)

# Extract Hop Data
hopcountPattern <- "^\\+.*Hop count: ([0-9]*)$"
hopData <- regmatches(rawData, gregexpr(hopcountPattern, rawData))
hopData <- hopData[grep(hopcountPattern, hopData)]
for (i in 1:length(hopData)) {
  hopData[i] <- sub(hopcountPattern, "\\1", hopData[i])
}

# Calculate RTT
df <- data.frame(sending = numeric(length(dataData)),
                 recieving = numeric(length(dataData)),
                 rtt = numeric(length(dataData)),
                 stringsAsFactors = FALSE)

for (i in 1:length(dataData)) {
  item <- strsplit(as.character(dataData[i]), " ")
  index <- as.numeric(item[[1]][2])
  recieveTime <- as.numeric(item[[1]][1])
  for (j in length(interestData):1) {
    item2 <- strsplit(as.character(interestData[j]), " ")
    index2 <- as.numeric(item2[[1]][2])
    sendTime <- as.numeric(item2[[1]][1])
    if (index2 - index == 0) {
      df$sending[index] <- sendTime
      df$recieving[index] <- recieveTime
      df$rtt[index] <- (recieveTime*1000000000 - sendTime*1000000000)# - 214296000
      break
    }
  }
}
plot(df$rtt[1:100], xlab="Data ID", ylab="Round-Trip Time")
rttsum <- summary(df$rtt[1:100])


# Calculate hop count
hopdf <- data.frame(hopnumber = numeric(length(hopData)),
                 stringsAsFactors = FALSE)
for (i in 1:length(hopData)) {
  hopdf$hopnumber[i] = as.numeric(hopData[i])
}
plot(hopdf$hopnumber[1:100], xlab="Data ID", ylab="Hop Number")