#pragma once

#include <pl/api.hpp>

#include <wolv/utils/guards.hpp>

#include <vector>
#include <span>
#include <memory>

namespace pl::hlp {

    class ProviderSection : public api::Section {
    public:
        using ReaderFunction = std::function<void(u64 fromAddress, u8* into, size_t size)>;
        using WriterFunction = std::function<void(u64 toAddress, const u8* from, size_t size)>;
        
        ProviderSection(size_t readBufferSize, size_t writeBufferSize);

        void setDataSize(u64 size) {
            m_dataSize = size;
        }
        void setReader(ReaderFunction reader) {
            m_reader = std::move(reader);
        }
        void setWriter(WriterFunction writer) {
            m_writer = std::move(writer);
        }
        
    protected:
        size_t size() const override {
            return m_dataSize;
        }
        
        IOError resize(size_t) override {
            return "ProviderSection cannot be resized";
        }

        IOError readRaw(u64 fromAddress, size_t size, ChunkReader reader) const override;

        IOError writeRaw(u64 toAddress, size_t size, ChunkWriter writer) override;
        
    private:
        u64 m_dataSize = 0x00;

        ReaderFunction m_reader;
        WriterFunction m_writer;
                
        mutable bool m_readBufferInUse = false;
        mutable bool m_writeBufferInUse = false;
        
        mutable std::vector<u8> m_readBuffer;
        mutable std::vector<u8> m_writeBuffer;
    };

    class InMemorySection : public api::Section {
    public:
        InMemorySection(size_t initialSize = 0, size_t maxSize = 0xFFFF'FFFF)
        : m_buffer(initialSize)
        , m_maxSize(maxSize)
        {}
        
    protected:
        size_t size() const override {
            return m_buffer.size();
        }
        
        IOError resize(size_t newSize) override {
            if (m_maxSize < newSize) {
                return fmt::format("Expansion beyond maximum size of {} is not permitted. Would overflow my {} bytes", m_maxSize, newSize - m_maxSize);
            }
            
            m_buffer.resize(newSize);
            return std::nullopt;
        }

        IOError readRaw(u64 address, size_t size, ChunkReader reader) const override {
            return reader(std::span(m_buffer).subspan(address, size));
        }
        
        IOError writeRaw(u64 address, size_t size, ChunkWriter writer) override {
            return writer(std::span(m_buffer).subspan(address, size));
        }

        std::vector<u8> m_buffer;
        size_t m_maxSize;
    };
    
}
