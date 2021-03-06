#pragma once

#include <Kernel/Devices/CharacterDevice.h>
#include <AK/Badge.h>
#include <Kernel/Lock.h>

class MasterPTY;

class PTYMultiplexer final : public CharacterDevice {
    AK_MAKE_ETERNAL
public:
    PTYMultiplexer();
    virtual ~PTYMultiplexer() override;

    static PTYMultiplexer& the();

    // ^CharacterDevice
    virtual KResultOr<Retained<FileDescriptor>> open(int options) override;
    virtual ssize_t read(Process&, byte*, ssize_t) override { return 0; }
    virtual ssize_t write(Process&, const byte*, ssize_t) override { return 0; }
    virtual bool can_read(Process&) const override { return true; }
    virtual bool can_write(Process&) const override { return true; }

    void notify_master_destroyed(Badge<MasterPTY>, unsigned index);

private:
    // ^CharacterDevice
    virtual const char* class_name() const override { return "PTYMultiplexer"; }

    Lock m_lock;
    Vector<unsigned> m_freelist;
};
