/***
 *
 *  Komodo Snapshot C++ Parser (c) Decker, 2020
 *
*/

#include <iostream>
#include <stdint.h>
#include <map>
#include <set>
#include <algorithm>
#include <functional>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"

#include "utilstrencodings.h"
#include "tinyformat.h"

using namespace rapidjson;

typedef int64_t CAmount;
typedef std::pair<std::string, CAmount> CBalanceRecord;
typedef std::vector< CBalanceRecord > CSendManyOutput;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

static const CAmount MAX_MONEY = 21000000 * COIN;
static const std::size_t MAX_SENDMANY_OUTPUTS = 10;

inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

int64_t AmountFromValue(const rapidjson::Value &value)
{
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

void PrintSendManyCli(std::ostream& ostr, const CSendManyOutput &vec, int64_t txcount, bool fProduceScriptEx = false)
{
    static const int64_t confirmations = 0;
    if (vec.size() > 0) {
        if (fProduceScriptEx) {
            ostr << "while true; do" << std::endl << "echo -e \"Executing tx \x1b[01;32m#"<< txcount << "\x1B[0m ... \"" << std::endl;
        }
        ostr << "./komodo-cli -ac_name=VOTE2020 sendmany \"\" \"{";
        for (CSendManyOutput::const_iterator iter = vec.begin(); iter != vec.end(); ++iter)
	    {
            if ((*iter).second > 0)
                ostr << "\\\"" << (*iter).first << "\\\":\\\"" << ValueFromAmount((*iter).second) << "\\\"" << ((std::distance( iter, vec.end() ) != 1) ? "," : "");
        }
        ostr << "}\" " << confirmations << " \"tx." << txcount << "\"" << std::endl;
        if (fProduceScriptEx) {
            ostr << "    if [ $? -eq 0 ]; then" << std::endl;
            ostr << "    break" << std::endl;
            ostr << "    fi" << std::endl;
            ostr << "    sleep 1" << std::endl;
            ostr << "done" << std::endl;
            ostr << std::endl;
        }
    }
}

int main()
{

    Document d;
    std::map<std::string, int64_t> mapBalances;
    
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

    // need to sort map by amounts before iterate
    CAmount total; int64_t txcount = 0, outcount = 0;
    CSendManyOutput vSendManyOutput;

    typedef std::function<bool(CBalanceRecord, CBalanceRecord)> Comparator;
    // Defining a lambda function to compare two pairs. 
    Comparator compFunctor =
        [](CBalanceRecord elem1, CBalanceRecord elem2)
        {
            //return (elem1.second > elem2.second);
            
            // First sort by balance ...
            if (elem1.second > elem2.second) return true;
            if (elem1.second < elem2.second) return false;
            // ... then by address, ...
            if (elem1.first > elem2.first) return true;
            if (elem1.first < elem2.first) return false;

        };

    // Declaring a set that will store the pairs using above comparision logic
    std::set<CBalanceRecord, Comparator> setOfBalances(
            mapBalances.begin(), mapBalances.end(), compFunctor);
    
    assert(mapBalances.size() == setOfBalances.size());

    //for (const auto& kv : mapBalances) {
    for (const CBalanceRecord& kv : setOfBalances) {

       if (vSendManyOutput.size() == MAX_SENDMANY_OUTPUTS) {
           // output komodo-cli sendmany
           txcount++; outcount += vSendManyOutput.size();
           PrintSendManyCli(std::cout, vSendManyOutput, txcount, false);
           vSendManyOutput.clear();
       }
       //vSendManyOutput.push_back( std::make_pair(kv.first, kv.second) );
       vSendManyOutput.emplace_back(kv.first, kv.second);
       total += kv.second;
    }

    if (vSendManyOutput.size() > 0) {
        txcount++; outcount += vSendManyOutput.size();
        PrintSendManyCli(std::cout, vSendManyOutput,txcount, false);
    }
    
    std::cerr << "Total: " << ValueFromAmount(total) << " coins sent to " << outcount << " addresses in " << txcount << " txes." << std::endl;

    return 0;
}
