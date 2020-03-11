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
           
    } 
*/