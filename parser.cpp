#include <iostream>
#include <stdint.h>
#include <map>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "utilstrencodings.h"
#include "tinyformat.h"

using namespace rapidjson;

inline void Ok() {
    std::cout << "Ok!" << std::endl;
}

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
    
    const char* test_json = "{\r\n        \"start_time\": 1581881335,\r\n        \"total\": 118392445.7801656,\r\n        \"average\": 1868.479159762006,\r\n        \"utxos\": 5685857,\r\n        \"total_addresses\": 63363,\r\n        \"ignored_addresses\": 31,\r\n        \"skipped_cc_utxos\": 0,\r\n        \"cc_utxo_value\": 0,\r\n        \"total_includeCCvouts\": 118392445.7801656,\r\n        \"ending_height\": 1752448,\r\n        \"addresses\": [\r\n            {\r\n            \"addr\": \"RLVzC4tr9cNKvuw2z4m8KuMfZURwCehx55\",\r\n            \"amount\": \"12419161.52338626\",\r\n            \"segid\": 27\r\n            },\r\n            {\r\n            \"addr\": \"RD8rqsm6RroQWe1cP45FvNNYJNT6Xsuvqj\",\r\n            \"amount\": \"0.00000054\",\r\n            \"segid\": 17\r\n            }\r\n        ],\r\n        \"end_time\": 1581881976\r\n    }";
    
    if (d.Parse(test_json).HasParseError()) {
        std::cerr << "JSON parse error. Exiting." << std::endl;
    }
    // https://github.com/Tencent/rapidjson/blob/master/example/tutorial/tutorial.cpp
    if (d.HasMember("addresses") && d["addresses"].IsArray()) {
        const Value& a = d["addresses"];
        std::cout << "Parsing " << a.Size() << " adresses ..." << std::endl;
        for (SizeType i = 0; i < a.Size(); i++) {
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

    std::cout << "Parsed " << mapBalances.size() << " addresses" << std::endl;
    
    // https://stackoverflow.com/questions/6963894/how-to-use-range-based-for-loop-with-stdmap
    
    CAmount total;
    for (const auto& kv : mapBalances) {
       //std::cout << kv.first << " has " << ValueFromAmount(kv.second) << std::endl;
       total += kv.second;
    }
    std::cout << "Total: " << ValueFromAmount(total) << " COIN" << std::endl;

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    std::cout << buffer.GetString() << std::endl;



    return 0;
}