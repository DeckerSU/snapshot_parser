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
typedef std::vector< std::pair <std::string, CAmount> > CSendManyOutput;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

static const CAmount MAX_MONEY = 21000000 * COIN;
static const std::size_t MAX_SENDMANY_OUTPUTS = 10;

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

void PrintSendManyCli(const CSendManyOutput &vec, int64_t txcount) {
    
    static const int64_t confirmations = 0;
    if (vec.size() > 0) {
        std::cout << "./komodo-cli -ac_name=VOTE2020 sendmany \"\" \"{";
        for (CSendManyOutput::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
	    {
            // https://stackoverflow.com/questions/3516196/testing-whether-an-iterator-points-to-the-last-item
            if ((*iter).second > 0)
                std::cout << "\\\"" << (*iter).first << "\\\":\\\"" << ValueFromAmount((*iter).second) << "\\\"" << ((std::distance( iter, vec.end() ) != 1) ? "," : "");
        }
        std::cout << "}\" " << confirmations << " \"tx." << txcount << "\"" << std::endl;
    }
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
    
    CAmount total; int64_t txcount = 0, outcount = 0;
    CSendManyOutput vSendManyOutput;

    for (const auto& kv : mapBalances) {
       
       if (vSendManyOutput.size() == MAX_SENDMANY_OUTPUTS) {
           // output komodo-cli sendmany
           txcount++; outcount += vSendManyOutput.size();
           PrintSendManyCli(vSendManyOutput, txcount);
           vSendManyOutput.clear();
       }
       //vSendManyOutput.push_back( std::make_pair(kv.first, kv.second) );
       vSendManyOutput.emplace_back(kv.first, kv.second);
       total += kv.second;
    }

    if (vSendManyOutput.size() > 0) {
        txcount++; outcount += vSendManyOutput.size();
        PrintSendManyCli(vSendManyOutput,txcount);
    }
    
    std::cerr << "Total: " << ValueFromAmount(total) << " coins sent to " << outcount << " addresses in " << txcount << " txes." << std::endl;

    /* {
        // Example of how to construct a JSON string
        Document jSendMany;
        Document::AllocatorType& a = jSendMany.GetAllocator();
        Value jParams, jAmounts;
        // {"jsonrpc": "1.0", "id":"curltest", "method": "sendmany", "params": ["", {"RD6GgnrMpPaTSMn8vai6yiGA7mN4QGPVMY":0.01,"RRyyejME7LRTuvdziWsXkAbSW1fdiohGwK":0.02}, 6, "testing"] }
        jSendMany.SetObject();
        jSendMany.AddMember("jsonrpc", "1.0", a);
        jSendMany.AddMember("id", "curltest", a);
        jSendMany.AddMember("method", "sendmany", a);
        jAmounts.SetObject();
        
        jParams.SetArray().PushBack("", a).PushBack(jAmounts,a); 
        jParams.PushBack(1, a); // minconf
        jParams.PushBack("testing",a); // comment

        jSendMany.AddMember("params",jParams,a);

        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        // writer.SetMaxDecimalPlaces(8); // https://github.com/Tencent/rapidjson/pull/536
        jSendMany.Accept(writer);
        std::cout << buffer.GetString() << std::endl; 
           
    } */

    return 0;
}