#ifndef __SLS_ERROR_H__
#define __SLS_ERROR_H__

namespace sls{

    class SLS_Error{
        public:
            std::string msg;
            SLS_Error(std::string message);
    };

    class No_Server{
    };

}

#endif
