#include "FileDescriptor.h"
#include "FileSystem.h"
#include "CharacterDevice.h"
#include <LibC/errno_numbers.h>
#include "UnixTypes.h"
#include <AK/BufferStream.h>

#ifdef SERENITY
#include "TTY.h"
#endif

RetainPtr<FileDescriptor> FileDescriptor::create(RetainPtr<VirtualFileSystem::Node>&& vnode)
{
    return adopt(*new FileDescriptor(move(vnode)));
}

FileDescriptor::FileDescriptor(RetainPtr<VirtualFileSystem::Node>&& vnode)
    : m_vnode(move(vnode))
{
}

FileDescriptor::~FileDescriptor()
{
}

RetainPtr<FileDescriptor> FileDescriptor::clone()
{
    auto descriptor = FileDescriptor::create(m_vnode.copyRef());
    if (!descriptor)
        return nullptr;
    descriptor->m_currentOffset = m_currentOffset;
#ifdef SERENITY
    descriptor->m_isBlocking = m_isBlocking;
#endif
    return descriptor;
}

#ifndef SERENITY
bool additionWouldOverflow(Unix::off_t a, Unix::off_t b)
{
    ASSERT(a > 0);
    uint64_t ua = a;
    return (ua + b) > maxFileOffset;
}
#endif

int FileDescriptor::stat(Unix::stat* buffer)
{
    if (!m_vnode)
        return -EBADF;

    auto metadata = m_vnode->metadata();
    if (!metadata.isValid())
        return -EIO;

    buffer->st_dev = 0; // FIXME
    buffer->st_ino = metadata.inode.index();
    buffer->st_mode = metadata.mode;
    buffer->st_nlink = metadata.linkCount;
    buffer->st_uid = metadata.uid;
    buffer->st_gid = metadata.gid;
    buffer->st_rdev = 0; // FIXME
    buffer->st_size = metadata.size;
    buffer->st_blksize = metadata.blockSize;
    buffer->st_blocks = metadata.blockCount;
    buffer->st_atime = metadata.atime;
    buffer->st_mtime = metadata.mtime;
    buffer->st_ctime = metadata.ctime;
    return 0;
}

Unix::off_t FileDescriptor::seek(Unix::off_t offset, int whence)
{
    if (!m_vnode)
        return -EBADF;

    // FIXME: The file type should be cached on the vnode.
    //        It's silly that we have to do a full metadata lookup here.
    auto metadata = m_vnode->metadata();
    if (!metadata.isValid())
        return -EIO;

    if (metadata.isSocket() || metadata.isFIFO())
        return -ESPIPE;

    Unix::off_t newOffset;

    switch (whence) {
    case SEEK_SET:
        newOffset = offset;
        break;
    case SEEK_CUR:
        newOffset = m_currentOffset + offset;
#ifndef SERENITY
        if (additionWouldOverflow(m_currentOffset, offset))
            return -EOVERFLOW;
#endif
        if (newOffset < 0)
            return -EINVAL;
        break;
    case SEEK_END:
        ASSERT(metadata.size); // FIXME: What do I do?
        newOffset = metadata.size;
        break;
    default:
        return -EINVAL;
    }

    m_currentOffset = newOffset;
    return m_currentOffset;
}

Unix::ssize_t FileDescriptor::read(byte* buffer, Unix::size_t count)
{
    if (m_vnode->isCharacterDevice()) {
        // FIXME: What should happen to m_currentOffset?
        return m_vnode->characterDevice()->read(buffer, count);
    }
    Unix::ssize_t nread = m_vnode->fileSystem()->readInodeBytes(m_vnode->inode, m_currentOffset, count, buffer, this);
    m_currentOffset += nread;
    return nread;
}

Unix::ssize_t FileDescriptor::write(const byte* data, Unix::size_t size)
{
    if (m_vnode->isCharacterDevice()) {
        // FIXME: What should happen to m_currentOffset?
        return m_vnode->characterDevice()->write(data, size);
    }
    // FIXME: Implement non-device writes.
    ASSERT_NOT_REACHED();
    return -1;
}

bool FileDescriptor::hasDataAvailableForRead()
{
    if (m_vnode->isCharacterDevice())
        return m_vnode->characterDevice()->hasDataAvailableForRead();
    return true;
}

ByteBuffer FileDescriptor::readEntireFile()
{
    if (m_vnode->isCharacterDevice()) {
        auto buffer = ByteBuffer::createUninitialized(1024);
        Unix::ssize_t nread = m_vnode->characterDevice()->read(buffer.pointer(), buffer.size());
        buffer.trim(nread);
        return buffer;
    }

    return m_vnode->fileSystem()->readEntireInode(m_vnode->inode, this);
}

bool FileDescriptor::isDirectory() const
{
    return m_vnode->metadata().isDirectory();
}

ssize_t FileDescriptor::get_dir_entries(byte* buffer, Unix::size_t size)
{
    auto metadata = m_vnode->metadata();
    if (!metadata.isValid())
        return -EIO;
    if (!metadata.isDirectory())
        return -ENOTDIR;

    // FIXME: Compute the actual size needed.
    auto tempBuffer = ByteBuffer::createUninitialized(2048);
    BufferStream stream(tempBuffer);
    m_vnode->vfs()->enumerateDirectoryInode(m_vnode->inode, [&stream] (auto& entry) {
        stream << (dword)entry.inode.index();
        stream << (byte)entry.fileType;
        stream << (dword)entry.name.length();
        stream << entry.name;
        return true;
    });

    if (size < stream.offset())
        return -1;

    memcpy(buffer, tempBuffer.pointer(), stream.offset());
    return stream.offset();
}
\
#ifdef SERENITY
bool FileDescriptor::isTTY() const
{
    if (auto* device = m_vnode->characterDevice())
        return device->isTTY();
    return false;
}

const TTY* FileDescriptor::tty() const
{
    if (auto* device = m_vnode->characterDevice())
        return static_cast<const TTY*>(device);
    return nullptr;
}

TTY* FileDescriptor::tty()
{
    if (auto* device = m_vnode->characterDevice())
        return static_cast<TTY*>(device);
    return nullptr;
}
#endif

int FileDescriptor::close()
{
    return 0;
}

String FileDescriptor::absolute_path() const
{
#ifdef SERENITY
    if (isTTY())
        return tty()->ttyName();
#endif
    return VirtualFileSystem::the().absolutePath(m_vnode->inode);
}