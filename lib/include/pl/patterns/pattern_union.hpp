#pragma once

#include <pl/patterns/pattern.hpp>

namespace pl {

    class PatternUnion : public Pattern,
                         public Inlinable {
    public:
        PatternUnion(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternUnion(const PatternUnion &other) : Pattern(other) {
            for (const auto &member : other.m_members) {
                auto copy = member->clone();

                this->m_sortedMembers.push_back(copy.get());
                this->m_members.push_back(std::move(copy));
            }
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternUnion(*this));
        }

        void forEachMember(const std::function<void(Pattern&)>& fn) {
            for (auto &member : this->m_sortedMembers)
                fn(*member);
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &member : this->m_members) {
                member->getHighlightedAddresses(highlight);
            }
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members)
                member->setOffset(member->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenColor())
                    member->setColor(color);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "union " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getMembers() const {
            return this->m_members;
        }

        void setMembers(std::vector<std::shared_ptr<Pattern>> &&members) {
            this->m_members.clear();
            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_sortedMembers.push_back(member.get());
                this->m_members.push_back(std::move(member));
            }
        }

        void sort(const std::function<bool (const Pattern *, const Pattern *)> &comparator) override {
            this->m_sortedMembers.clear();
            for (auto &member : this->m_members)
                this->m_sortedMembers.push_back(member.get());

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), comparator);

            for (auto &member : this->m_members)
                member->sort(comparator);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherUnion = *static_cast<const PatternUnion *>(&other);
            if (this->m_members.size() != otherUnion.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherUnion.m_members[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] Pattern *getPattern(u64 offset) override {
            if (this->isHidden()) return nullptr;

            auto largestMember = std::find_if(this->m_members.begin(), this->m_members.end(), [this](const std::shared_ptr<Pattern> &pattern) {
                return pattern->getSize() == this->getSize();
            });

            if (largestMember == this->m_members.end())
                return nullptr;
            else
                return (*largestMember)->getPattern(offset);
        }

        void setEndian(std::endian endian) override {
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
            }

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        std::string getFormattedValue() override {
            return this->formatDisplayValue("{ ... }", this);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_members;
        std::vector<Pattern *> m_sortedMembers;
    };

}