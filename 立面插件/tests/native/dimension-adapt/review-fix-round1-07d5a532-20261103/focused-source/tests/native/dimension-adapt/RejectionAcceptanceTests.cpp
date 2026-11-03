#include "RejectionAcceptance.h"
#include <iostream>

void require_test(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    try {
        bool normalRejected=false;
        try {n8a::expected_rejection("source_sha_mismatch",[]{},"source_sha_mismatch");}
        catch(const std::runtime_error& failure){normalRejected=std::string(failure.what()).find("fault_was_not_rejected:")==0;}
        require_test(normalRejected,"helper accepted normal return when name equals expected");
        const auto expected=n8a::expected_rejection("formal_source_control",[]{throw std::runtime_error("reference source SHA mismatch");},"reference source SHA mismatch");
        require_test(expected=="reference source SHA mismatch","helper must retain the actual operation exception");
        bool wrongStageRejected=false;
        try {n8a::expected_rejection("formal_source_control",[]{throw std::runtime_error("module SHA mismatch");},"reference source SHA mismatch");}
        catch(const std::runtime_error& failure){wrongStageRejected=std::string(failure.what()).find("fault failed before intended stage")==0;}
        require_test(wrongStageRejected,"helper accepted an exception from the wrong stage");
        std::cout<<"N8A_REJECTION_ACCEPTANCE_TESTS_PASSED: normal return rejected; expected exception retained; wrong stage rejected\n";return 0;
    }catch(const std::exception& failure){std::cerr<<"N8A_REJECTION_ACCEPTANCE_TESTS_FAILED: "<<failure.what()<<'\n';return 1;}
}
