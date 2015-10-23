#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>

template <typename EntityBase>
class EntityBag {
    class iterator;
    friend class iterator;
    struct TypeBagBase {
        TypeBagBase(size_t entitySize_) : entitySize{entitySize_} {}
        virtual ~TypeBagBase() {}
        virtual void emplace(const EntityBase& x) = 0;
        auto size() { return numElems; }
        EntityBase* entity(size_t index) {
            return reinterpret_cast<EntityBase*>(baseElems + index * entitySize);
        }

    protected:
        uintptr_t baseElems = 0;
        size_t numElems = 0;
        size_t entitySize = 0;
    };
    template <typename T>
    struct TypeBag : public TypeBagBase {
        TypeBag() : TypeBagBase{sizeof(T)} {}
        virtual void emplace(const EntityBase& x) { emplace(static_cast<const T&>(x)); }
        template <typename... Args>
        void emplace(Args&&... args) {
            elems.emplace_back(std::forward<Args>(args)...);
            baseElems = reinterpret_cast<uintptr_t>(elems.data());
            numElems = elems.size();
        }
        std::vector<T> elems;
    };
    using TypeBagMap = std::unordered_map<std::type_index, std::unique_ptr<TypeBagBase>>;
    using TypeBagMapConstIterator = typename TypeBagMap::const_iterator;

public:
    class iterator : std::iterator<std::forward_iterator_tag, EntityBase> {
    public:
        iterator(TypeBagMapConstIterator it) : currentTypeBag{it} {}
        bool operator==(const iterator& x) {
            return std::tie(currentTypeBag, currentElementIndex) ==
                   std::tie(x.currentTypeBag, x.currentElementIndex);
        }
        bool operator!=(const iterator& x) { return !(*this == x); }
        EntityBase* operator*() { return currentTypeBag->second->entity(currentElementIndex); }
        iterator& operator++() {
            if (currentElementIndex < currentTypeBag->second->size() - 1) {
                ++currentElementIndex;
            } else {
                ++currentTypeBag;
                currentElementIndex = 0;
            }
            return *this;
        }

    private:
        TypeBagMapConstIterator currentTypeBag;
        size_t currentElementIndex = 0;
    };

    template <typename T, typename... Args>
    void emplace(Args&&... args) {
        auto& typeBagBaseUp = typeBags[typeid(T)];
        if (!typeBagBaseUp) typeBagBaseUp = std::make_unique<TypeBag<T>>();
        auto& typeBag = static_cast<TypeBag<T>&>(*typeBagBaseUp);
        typeBag.emplace(args...);
    }

    // Insert a copy of x with concrete type T dynamically worked out using RTTI, this requires a
    // TypeBag<T> already exists in the map since in general we don't have a way to create one
    // otherwise.
    void emplace(const EntityBase& x) {
        auto findIt = typeBags.find(typeid(x));
        if (findIt != typeBags.end()) {
            findIt->second->emplace(x);
        } else {
            auto msg =
                std::string{"Could not find an existing type bag for type "} + typeid(x).name();
            assert(false && msg.c_str());
        }
    }

    iterator begin() const { return iterator{typeBags.begin()}; }
    iterator end() const { return iterator{typeBags.end()}; }

private:
    TypeBagMap typeBags;
};

class Foo {
public:
    virtual ~Foo() {}
    virtual void update() = 0;
};

class Bar : public Foo {
public:
    Bar(int i_, float f_) : i{i_}, f{f_} {}
    virtual void update() override {
        std::cout << "Bar::update(): i = " << i << ", f = " << f << '\n';
    }

private:
    int i;
    float f;
};

class Baz : public Foo {
public:
    Baz(float f_) : f{f_} {}
    virtual void update() override { std::cout << "Baz::update(): f = " << f << '\n'; }

private:
    float f;
};

int main() {
    EntityBag<Foo> entityBag;
    entityBag.emplace<Bar>(1, 2.0f);
    entityBag.emplace<Baz>(1.0f);
    entityBag.emplace<Bar>(2, 4.0f);
    entityBag.emplace<Bar>(3, 5.0f);
    entityBag.emplace<Baz>(2.0f);
    entityBag.emplace<Bar>(4, 6.0f);
    entityBag.emplace<Baz>(3.0f);
    Baz aBaz{4.0f};
    Foo& aFooRef = aBaz;
    entityBag.emplace(aFooRef);
    for (auto e : entityBag) {
        e->update();
    }
}
