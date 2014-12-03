#ifndef __SLS_ERROR_H__
#define __SLS_ERROR_H__

#include <string>

namespace sls{

    class SLS_Error{
        public:
            std::string msg;
            SLS_Error(const std::string &message);
    };

    class SLS_No_Server{
    };

}

#endif
