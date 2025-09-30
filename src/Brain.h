#pragma once

#include "Arduino.h"
#include "RollingAverage.h"
#include "config.h"

#define MAX_PACKET_LENGTH 32
#define EEG_POWER_BANDS 8

class Brain {
    public:
        Brain(const char* sName);

        uint8_t getAverage();

        // Run this in the main loop.
        //boolean update(uint8_t* pBuf, int length);
        boolean update(uint8_t update_byte);

        // String with most recent error.
        char* readErrors();

        // Returns comme-delimited string of all available brain data.
        char* readCSV();

        // Individual pieces of brain data.
        uint8_t readSignalQuality();
        uint8_t readAttention();
        uint8_t readMeditation();
        uint32_t* readPowerArray();
        uint32_t readDelta();
        uint32_t readTheta();
        uint32_t readLowAlpha();
        uint32_t readHighAlpha();
        uint32_t readLowBeta();
        uint32_t readHighBeta();
        uint32_t readLowGamma();
        uint32_t readMidGamma();
    
        const char* sName;
        uint8_t signalQuality;
        uint8_t signalQualityNotEstimated;
        uint8_t meditation;
        uint8_t attention;

    private:
        //static RunningMedian AttentionAvg;
        Stream* brainStream;
        uint8_t packetData[MAX_PACKET_LENGTH];
        boolean inPacket;
        uint8_t latestByte;
        uint8_t lastByte;
        uint8_t packetIndex;
        uint8_t packetLength;
        uint8_t checksum;
        uint8_t checksumAccumulator;
        uint8_t eegPowerLength;
        boolean hasPower;
        void clearPacket();
        void clearEegPower();
        boolean parsePacket();

        void printPacket();
        void init();
        void printCSV(); // maybe should be public?
        void printDebug();

        RollingAverage<uint8_t> attentionAvg = RollingAverage<uint8_t>(averagingLength); // 10-sample window (adjust as needed)

        //char csvBuffer[100];
        boolean freshPacket;

        //
        uint32_t eegPower[EEG_POWER_BANDS];
};
