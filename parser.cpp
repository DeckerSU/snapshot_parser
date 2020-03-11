#include <iostream>
#include <stdint.h>
#include <map>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"

#include "utilstrencodings.h"
#include "tinyformat.h"

using namespace rapidjson;

typedef int64_t CAmount;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

static const CAmount MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

int64_t AmountFromValue(const rapidjson::Value &value) {
    if (!value.IsString()) throw std::runtime_error("Amount is not string");
    CAmount amount;
    if (!ParseFixedPoint(value.GetString(), 8, &amount)) throw std::runtime_error("Invalid amount");
    if (!MoneyRange(amount)) throw std::runtime_error("Amount out of range");
    return amount;
}

std::string ValueFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder);
}

int main() {

    Document d;
    std::map<std::string, uint64_t> mapBalances;
    
    // Prepare JSON reader and input stream.
    FILE *fp = fopen("snapshot.json", "r");
    if (!fp) {
        std::cerr << "Can't find snapshot.json. Exiting." << std::endl;
        return -1;
    }
    
    Reader reader;
    char readBuffer[65536];
    FileReadStream is(fp /*stdin*/, readBuffer, sizeof(readBuffer));

    if (d.ParseStream(is).HasParseError()) {
        std::cerr << "JSON parse error. Exiting." << std::endl;
        fclose(fp);
        return -1;
    }

    // https://github.com/Tencent/rapidjson/blob/master/example/tutorial/tutorial.cpp
    int64_t totalAddressesInJson = 0;
    if (d.HasMember("addresses") && d["addresses"].IsArray()) {
        const Value& a = d["addresses"];
        totalAddressesInJson = a.Size();
        std::cerr << "Parsing " << totalAddressesInJson << " adresses - ";
        for (SizeType i = 0; i < totalAddressesInJson; i++) {
            if (a[i].IsObject()) {
                const Value& el = a[i];
                if (el.HasMember("addr") && el.HasMember("amount")) {
                    if (el["addr"].IsString() && el["amount"].IsString()) {
                        mapBalances[el["addr"].GetString()] = AmountFromValue(el["amount"]);
                    }
                }
            }
        }

    }

    if (totalAddressesInJson != mapBalances.size()) {
        std::cerr << "only " << mapBalances.size() << " valid! Exiting." << std::endl;
        return -1;
    }

    std::cerr << "Ok!" << std::endl;
    
    // https://stackoverflow.com/questions/6963894/how-to-use-range-based-for-loop-with-stdmap
    
    CAmount total;
    for (const auto& kv : mapBalances) {
       //std::cout << kv.first << " has " << ValueFromAmount(kv.second) << std::endl;
       total += kv.second;
    }
    std::cout << "Total: " << ValueFromAmount(total) << std::endl;

    /* StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    std::cout << buffer.GetString() << std::endl; */

    return 0;
}