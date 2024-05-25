#include <pl/core/sections.hpp>

#include <pl/core/evaluator.hpp>

#include <fmt/format.h>

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

    void ViewSection::addSectionSpan(u64 sectionId, u64 fromAddress, size_t size, std::optional<u64> atOffset) {
        if (atOffset == std::nullopt) {
            if (m_spans.empty()) {
                atOffset = 0;
            } else {
                auto last = std::prev(m_spans.end());
                atOffset = last->first + last->second.size;
            }
        }
        
        m_spans.emplace(*atOffset, SectionSpan{
            .sectionId = sectionId,
            .offset = fromAddress,
            .size = size
        });
    }

    size_t ViewSection::size() const {
        if (m_spans.empty()) {
            return 0;
        }
        
        auto last = std::prev(m_spans.end());
        return (last->first + last->second.size) - m_spans.begin()->first;
    }

    ViewSection::IOError ViewSection::resize(size_t) {
        return "Not implemented";
    }

    ViewSection::IOError ViewSection::readRaw(u64 fromAddress, size_t size, ChunkReader& reader) const {
        return access<true>(fromAddress, size, reader);
    }

    ViewSection::IOError ViewSection::writeRaw(u64 toAddress, size_t size, ChunkWriter& writer) {
        return access<false>(toAddress, size, writer);
    }

    template<bool IsRead>
    ViewSection::IOError ViewSection::access(u64 address, size_t size, auto& readerOrWriter) const {
        if (m_isBeingAccessed) {
            return "View self-recursion not permitted";
        }
        
        m_isBeingAccessed = true;
        ON_SCOPE_EXIT { m_isBeingAccessed = false; };
        
        const auto fail = [](u64 from, u64 to, const std::string& extra) {
            return fmt::format("Attempted to access out-of-bounds area 0x{:X}-0x{:X} (of {} bytes). {}", from, to, to - from, extra);
        };
        
        const auto outOfBounds = [&](u64 from, u64 to) {
            std::string hint;
            
            if (m_spans.empty()) {
                hint.append("ViewSection is empty!");
            } else {
                auto upperBound = m_spans.upper_bound(from);
                
                auto before = std::prev(upperBound);
                if (from < before->first) {
                    auto endsAt = before->first + before->second.size;
                    
                    hint.append(fmt::format("Last mapped area before ends at 0x{:X} ({} bytes away).", endsAt, to - endsAt));
                }

                if (upperBound != m_spans.end()) {
                    auto startsAt = upperBound->first;
                    
                    hint.append((hint.size() == 0) ? "" : " ");
                    hint.append(fmt::format("First mapped area after starts at 0x{:X} ({} bytes away).", startsAt, startsAt - from));
                }
            }

            return fail(from, to, hint);
        };
        
        if (m_spans.empty()) [[unlikely]] {
            return outOfBounds(address, address + size);
        }
        
        while(true) {
            auto it = std::prev(m_spans.upper_bound(address));
            
            if (address < it->first) {
                return outOfBounds(address, address + size);
            }
            
            // Doing this check late ensures that even 0 size accesses to
            //  outside of the mapped section spans are considered errors
            if (size == 0) [[unlikely]] {
                return std::nullopt;
            }
            
            const SectionSpan& span = it->second;
        
            const auto chunkOffset = address - span.offset;
            const auto chunkSize = std::min(size, span.size);
            
            try {
                auto& section = m_evaluator.getSection(span.sectionId);
                
                if constexpr (IsRead) {
                    if (auto error = section.read(chunkOffset, chunkSize, readerOrWriter)) {
                        return fmt::format("Error writing underlying section {}: {}", span.sectionId, *error);
                    }
                } else {
                    if (auto error = section.write(false, chunkOffset, chunkSize, readerOrWriter)) {
                        return fmt::format("Error writing underlying section {}: {}", span.sectionId, *error);
                    }
                }
            } catch (...) {
                return fail(address, address + chunkSize, fmt::format("Failed to access mapped section {}", span.sectionId));
            }

            address += chunkSize;
            size -= chunkSize;
            
            // No point in repeating access checks
            if (size == 0) {
                return std::nullopt;
            }
        }
    }
    
}
