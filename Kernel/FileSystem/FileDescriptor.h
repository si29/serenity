#pragma once

#include <Kernel/FileSystem/VirtualFileSystem.h>
#include <Kernel/FileSystem/InodeMetadata.h>
#include <Kernel/FileSystem/FIFO.h>
#include <Kernel/LinearAddress.h>
#include <AK/ByteBuffer.h>
#include <AK/CircularQueue.h>
#include <AK/Retainable.h>
#include <AK/Badge.h>
#include <Kernel/Net/Socket.h>
#include <Kernel/SharedMemory.h>

class TTY;
class MasterPTY;
class Process;
class Region;
class CharacterDevice;

class FileDescriptor : public Retainable<FileDescriptor> {
public:

    static Retained<FileDescriptor> create(RetainPtr<Socket>&&, SocketRole = SocketRole::None);
    static Retained<FileDescriptor> create(RetainPtr<Inode>&&);
    static Retained<FileDescriptor> create(RetainPtr<Device>&&);
    static Retained<FileDescriptor> create(RetainPtr<SharedMemory>&&);
    static Retained<FileDescriptor> create_pipe_writer(FIFO&);
    static Retained<FileDescriptor> create_pipe_reader(FIFO&);
    ~FileDescriptor();

    Retained<FileDescriptor> clone();

    int close();

    off_t seek(off_t, int whence);
    ssize_t read(Process&, byte*, ssize_t);
    ssize_t write(Process&, const byte* data, ssize_t);
    KResult fstat(stat&);

    KResult fchmod(mode_t);

    bool can_read(Process&);
    bool can_write(Process&);

    ssize_t get_dir_entries(byte* buffer, ssize_t);

    ByteBuffer read_entire_file(Process&);

    KResultOr<String> absolute_path();

    bool is_directory() const;

    bool is_device() const { return m_device.ptr(); }
    Device* device() { return m_device.ptr(); }
    const Device* device() const { return m_device.ptr(); }

    bool is_block_device() const;
    bool is_character_device() const;
    CharacterDevice* character_device();
    const CharacterDevice* character_device() const;

    bool is_tty() const;
    const TTY* tty() const;
    TTY* tty();

    bool is_master_pty() const;
    const MasterPTY* master_pty() const;
    MasterPTY* master_pty();

    InodeMetadata metadata() const;
    Inode* inode() { return m_inode.ptr(); }
    const Inode* inode() const { return m_inode.ptr(); }

    bool supports_mmap() const;
    Region* mmap(Process&, LinearAddress, size_t offset, size_t, int prot);

    bool is_blocking() const { return m_is_blocking; }
    void set_blocking(bool b) { m_is_blocking = b; }

    dword file_flags() const { return m_file_flags; }
    void set_file_flags(dword flags) { m_file_flags = flags; }

    bool is_socket() const { return m_socket; }
    Socket* socket() { return m_socket.ptr(); }
    const Socket* socket() const { return m_socket.ptr(); }

    bool is_fifo() const { return m_fifo; }
    FIFO::Direction fifo_direction() { return m_fifo_direction; }

    bool is_file() const;
    bool is_shared_memory() const { return m_shared_memory; }
    SharedMemory* shared_memory() { return m_shared_memory.ptr(); }
    const SharedMemory* shared_memory() const { return m_shared_memory.ptr(); }

    ByteBuffer& generator_cache() { return m_generator_cache; }

    void set_original_inode(Badge<VFS>, Retained<Inode>&& inode) { m_inode = move(inode); }

    SocketRole socket_role() const { return m_socket_role; }
    void set_socket_role(SocketRole);

    KResult truncate(off_t);

private:
    friend class VFS;
    FileDescriptor(RetainPtr<Socket>&&, SocketRole);
    explicit FileDescriptor(RetainPtr<Inode>&&);
    explicit FileDescriptor(RetainPtr<Device>&&);
    explicit FileDescriptor(RetainPtr<SharedMemory>&&);
    FileDescriptor(FIFO&, FIFO::Direction);

    RetainPtr<Inode> m_inode;
    RetainPtr<Device> m_device;

    off_t m_current_offset { 0 };

    ByteBuffer m_generator_cache;

    bool m_is_blocking { true };
    dword m_file_flags { 0 };

    RetainPtr<Socket> m_socket;
    SocketRole m_socket_role { SocketRole::None };

    RetainPtr<FIFO> m_fifo;
    FIFO::Direction m_fifo_direction { FIFO::Neither };

    RetainPtr<SharedMemory> m_shared_memory;

    bool m_closed { false };
};

