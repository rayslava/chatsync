#include <list>
#include <memory>

#include "channel.hpp"

namespace Hub {

    typedef std::unique_ptr<Channeling::Channel const> chanPtr;

    /**
    * A thread-safe implementation of two connected channels sets.
    * All input channels are redirected to every output channel.
    */
    class Hub {
    private:
        const std::string _name;             /**< Human-readable name */
        std::list<chanPtr> _inputChannels;   /**< Container for all input channels */
        std::list<chanPtr> _outputChannels;  /**< Container for all output channels */

    public:
        Hub(std::string const& name);

        const std::string &name() const {return _name;};

        /**
        * Append one more input channel to list taking ownership
        */
        void addInput(Channeling::Channel const*);

        /**
        * Append one more output channel to list taking ownership
        */
        void addOutput(Channeling::Channel const*);
    };
}