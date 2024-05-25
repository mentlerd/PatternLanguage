#include <pl/core/sections.hpp>

namespace pl::hlp {

    ProviderSection::ProviderSection(size_t readBufferSize, size_t writeBufferSize)
    : m_readBuffer(readBufferSize)
    , m_writeBuffer(writeBufferSize)
    {}

    ProviderSection::IOError ProviderSection::readRaw(u64 fromAddress, size_t size, ChunkReader reader) const {
        if (!m_reader) {
            return "No memory has been attached. Reading is disabled";
        }
        if (m_readBufferInUse) {
            return "Reentrant read operations are not supported";
        }
        if (m_readBuffer.size() == 0) {
            return "Zero size read buffer prevents reading";
        }
        
        m_readBufferInUse = true;
        ON_SCOPE_EXIT { m_readBufferInUse = false; };
        
        while(size_t chunkSize = std::min(size, m_readBuffer.size())) {
            std::span<u8> span = std::span(m_readBuffer).subspan(0, chunkSize);
            
            m_reader(fromAddress, span.data(), span.size());
            
            if (auto error = reader(span)) {
                return error;
            }
            
            fromAddress += chunkSize;
            size -= chunkSize;
        }
        
        return std::nullopt;
    }

    ProviderSection::IOError ProviderSection::writeRaw(u64 toAddress, size_t size, ChunkWriter writer) {
        if (!m_writer) {
            return "No memory has been attached. Writing is disabled";
        }
        if (m_readBufferInUse) {
            return "Reentrant write operations are not supported";
        }
        if (m_readBuffer.size() == 0) {
            return "Zero size write buffer prevents writing";
        }
        
        m_writeBufferInUse = true;
        ON_SCOPE_EXIT { m_writeBufferInUse = false; };
        
        while(size_t chunkSize = std::min(size, m_writeBuffer.size())) {
            std::span<u8> span = std::span(m_writeBuffer).subspan(0, chunkSize);
            
            if (auto error = writer(span)) {
                return error;
            }
            
            m_writer(toAddress, span.data(), span.size());
            
            toAddress += chunkSize;
            size -= chunkSize;
        }
        
        return std::nullopt;
    }

}
