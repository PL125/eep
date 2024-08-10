#include <Arduino.h>
#include "M93Cx6.h"

#define PWR_PIN 9
#define CS_PIN 10
#define SK_PIN 7
#define DI_PIN 11
#define DO_PIN 12
#define ORG_PIN 8

#define WIRE_VERSION "v2.0.16\n"

M93Cx6 ep = M93Cx6(PWR_PIN, CS_PIN, SK_PIN, DO_PIN, DI_PIN, ORG_PIN, 150);

static uint8_t cfgChip = 66;
static uint16_t cfgSize = 512;
static uint8_t cfgOrg = 8;
static uint8_t cfgDelay = 150;

const uint8_t BUFF_SIZE = 16;      // make it big enough to hold your longest command
static char buffer[BUFF_SIZE + 1]; // +1 allows space for the null terminator
static uint8_t bufferLength;       // number of characters currently in the buffer

void setup()
{
    Serial.begin(1000000);
    pinMode(LED_BUILTIN, OUTPUT); // LED
    while (!Serial)
    {
        // wait for serial port to connect. Needed for native USB
    };

    Serial.write(WIRE_VERSION);
}

void loop()
{
    if (Serial.available())
    {
        char c = Serial.read();
        if ((c == '\r' && bufferLength == 0))
        {
            help();
            bufferLength = 0;
            return;
        }

        if ((c == '\r') || (c == '\n'))
        {
            // end-of-line received
            if (bufferLength > 0)
            {
                handleCmd();
            }
            bufferLength = 0;
            return;
        }

        if (c == 127) // handle backspace during command input
        {
            if (bufferLength > 0)
            {
                bufferLength--;
            }
            return;
        }

        if (bufferLength < BUFF_SIZE)
        {
            buffer[bufferLength++] = c;  // append the received character to the array
            buffer[bufferLength] = 0x00; // append the null terminator
        }
        else
        {
            Serial.write('\a');
        }
    }
}

void handleCmd()
{
    switch (buffer[0])
    {
    case 'v':
        Serial.print(WIRE_VERSION);
        return;
    case 'h':
        help();
        return;
    case 's':
        parseBuffer();
        Serial.write("\n");
        return;
    case 'w':
    case 'r':
    case 'e':
    case 'p':
    case 'x':
        break;
    case '?':
        settings();
        return;
    default:
        Serial.println("invalid command");
        help();
        return;
    }

    if (bufferLength > 8) // parse long command with set options inline
    {
        parseBuffer();
    }

    ep.powerUp();
    delay(20);
    switch (buffer[0])
    {
    case 'r':
        read();
        break;
    case 'w':
        write();
        break;
    case 'p':
        printBin();
        break;
    case 'e':
        erase();
        break;
    }
    ep.powerDown();
}

void parseBuffer()
{
    char *cmd;
    char *pos;
    cmd = strtok(buffer, ",");

    pos = strtok(NULL, ",");
    cfgChip = atoi(pos);

    pos = strtok(NULL, ",");
    cfgSize = atoi(pos);

    pos = strtok(NULL, ",");
    cfgOrg = atoi(pos);

    pos = strtok(NULL, ",");
    cfgDelay = atoi(pos);

    if (cfgSize == 0)
    {
        Serial.println("\ainvalid size");
        return;
    }

    if (!setChip())
    {
        return;
    }

    if (!setOrg())
    {
        return;
    }

    if (!setDelay())
    {
        return;
    }
}

bool setChip()
{
    switch (cfgChip)
    {
    case 46:
        ep.setChip(M93C46);
        break;
    case 56:
        ep.setChip(M93C56);
        break;
    case 66:
        ep.setChip(M93C66);
        break;
    case 76:
        ep.setChip(M93C76);
        break;
    case 86:
        ep.setChip(M93C86);
        break;
    default:
        Serial.println("\ainvalid chip");
        return false;
    }

    return true;
}

bool setOrg()
{
    switch (cfgOrg)
    {
    case 8:
        ep.setOrg(ORG_8);
        break;
    case 16:
        ep.setOrg(ORG_16);
        break;
    default:
        Serial.println("\ainvalid org");
        return false;
    }

    return true;
}

bool setDelay()
{
    ep.setPinDelay(cfgDelay);
    return true;
}

void help()
{
    Serial.println("--- eep ---");
    Serial.println("s,<chip>,<size>,<org>,<pin_delay> - Set configuration");
    Serial.println("? - Print current configuration");
    Serial.println("r - Read eeprom");
    Serial.println("w - Initiate write mode");
    Serial.println("e - Erase eeprom");
    Serial.println("p - Hex print eeprom content");
    Serial.println("h - This help");
    Serial.println("v - Print version");
}

void settings()
{
    Serial.println("--- settings ---");
    Serial.print("chip: ");
    Serial.println(cfgChip);
    Serial.print("size: ");
    Serial.println(cfgSize);
    Serial.print("org: ");
    Serial.println(cfgOrg);
    Serial.print("delay: ");
    Serial.println(cfgDelay);
}

void read()
{
    delay(20);
    bufferLength = 0;
    for (uint16_t i = 0; i < cfgSize; i++)
    {
        uint16_t read = ep.read(i);
        if (cfgOrg == 8)
        {
            Serial.write(read);
        }
        else
        {
            Serial.write(read >> 8);
            Serial.write(read & 0xFF);
        }
        /*
                buffer[bufferLength++] = ep.read(i);
                buffer[bufferLength] = 0x00;
                if (bufferLength == BUFF_SIZE)
                {
                    ledOn();
                    for (uint8_t j = 0; j < bufferLength; j++)
                    {
                        Serial.write(buffer[j]);
                    }
                    bufferLength = 0;
                    ledOff();
                }
        */
    }
}

void write()
{
    long lastData = millis();
    ep.writeEnable();
    Serial.write('\f');
    bufferLength = 0;
    uint16_t writePos = 0;
    for (;;)
    {
        if ((millis() - lastData) > 500)
        {
            Serial.println("\adata read timeout");
            break;
        }
        ledOn();
        while (Serial.available() > 0)
        {
            buffer[bufferLength++] = Serial.read();
            buffer[bufferLength] = 0x00;
            lastData = millis();
            if (bufferLength == BUFF_SIZE)
            {
                for (uint8_t j = 0; j < bufferLength; j++)
                {
                    ep.write(writePos++, buffer[j]);
                }
                Serial.print("\f");
                bufferLength = 0;
            }
        }
        ledOff();
        if (writePos >= cfgSize)
        {
            break;
        }
    }
    ep.writeDisable();
    Serial.print("\r\n--- write done ---");
}

void erase()
{
    ep.writeEnable();
    ep.eraseAll();
    ep.writeDisable();
    Serial.println("\aeeprom erased");
}

void printBin()
{
    uint8_t linePos = 0;
    char buf[3];
    ledOn();
    Serial.println("--- Hex dump ---");
    for (uint16_t i = 0; i < cfgSize; i++)
    {
        uint8_t c = ep.read(i);
        sprintf(buf, "%02X ", c);
        Serial.print(buf);
        linePos++;
        if (linePos == 24)
        {
            Serial.write("\n");
            linePos = 0;
        }
    }
    Serial.write("\n");
    ledOff();
}

void ledOn()
{
    // digitalWrite(LED_BUILTIN, HIGH);
    PORTB |= (B00000001 << (13 - 8));
}

void ledOff()
{
    // digitalWrite(LED_BUILTIN, LOW);
    PORTB &= (~(B00000001 << (13 - 8)));
}
