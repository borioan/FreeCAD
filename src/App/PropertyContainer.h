/***************************************************************************
 *   Copyright (c) 2005 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#ifndef SRC_APP_PROPERTYCONTAINER_H_
#define SRC_APP_PROPERTYCONTAINER_H_

#include <map>
#include <cstring>
#include <vector>
#include <string>
#include <Base/Persistence.h>

#include "DynamicProperty.h"

namespace Base {
class Writer;
}


namespace App
{
class Property;
class PropertyContainer;
class DocumentObject;
class Extension;

// clang-format off
enum PropertyType
{
    Prop_None        = 0, /*!< No special property type */
    Prop_ReadOnly    = 1, /*!< Property is read-only in the editor */
    Prop_Transient   = 2, /*!< Property content won't be saved to file, but still saves name, type and status */
    Prop_Hidden      = 4, /*!< Property won't appear in the editor */
    Prop_Output      = 8, /*!< Modified property doesn't touch its parent container */
    Prop_NoRecompute = 16,/*!< Modified property doesn't touch its container for recompute */
    Prop_NoPersist   = 32,/*!< Property won't be saved to file at all */
};
// clang-format on

struct AppExport PropertyData
{
  struct PropertySpec
  {
    const char * Name;
    const char * Group;
    const char * Docu;
    short Offset, Type;

    inline PropertySpec(const char *name, const char *group, const char *doc, short offset, short type)
        :Name(name),Group(group),Docu(doc),Offset(offset),Type(type)
    {}
  };

  //purpose of this struct is to be constructible from all acceptable container types and to
  //be able to return the offset to a property from the accepted containers. This allows you to use
  //one function implementation for multiple container types without losing all type safety by
  //accepting void*
  struct OffsetBase
  {
      // Lint wants these marked explicit, but they are currently used implicitly in enough
      // places that I don't wnt to fix it. Instead we disable the Lint message.
      // NOLINTNEXTLINE(runtime/explicit)
      OffsetBase(const App::PropertyContainer* container) : m_container(container) {}
      // NOLINTNEXTLINE(runtime/explicit)
      OffsetBase(const App::Extension* container) : m_container(container) {}

      short int getOffsetTo(const App::Property* prop) const {
            auto *pt = (const char*)prop;
            auto *base = (const char *)m_container;
            if(pt<base || pt>base+SHRT_MAX)
                return -1;
            return (short) (pt-base);
      }
      char* getOffset() const {return (char*) m_container;}

  private:
      const void* m_container;
  };

    // clang-format off
    // A multi index container for holding the property spec, with the following
    // index,
    // * a sequence, to preserve creation order
    // * hash index on property name
    // * hash index on property pointer offset
    mutable bmi::multi_index_container<
        PropertySpec,
        bmi::indexed_by<
            bmi::sequenced<>,
            bmi::hashed_unique<
                bmi::member<PropertySpec, const char*, &PropertySpec::Name>,
                CStringHasher,
                CStringHasher
            >,
            bmi::hashed_unique<
                bmi::member<PropertySpec, short, &PropertySpec::Offset>
            >
        >
    > propertyData;
    // clang-format on

  mutable bool parentMerged = false;

  const PropertyData*     parentPropertyData;

  void addProperty(OffsetBase offsetBase,const char* PropName, Property *Prop, const char* PropertyGroup= nullptr, PropertyType = Prop_None, const char* PropertyDocu= nullptr );

  const PropertySpec *findProperty(OffsetBase offsetBase,const char* PropName) const;
  const PropertySpec *findProperty(OffsetBase offsetBase,const Property* prop) const;

  const char* getName         (OffsetBase offsetBase,const Property* prop) const;
  short       getType         (OffsetBase offsetBase,const Property* prop) const;
  short       getType         (OffsetBase offsetBase,const char* name)     const;
  const char* getGroup        (OffsetBase offsetBase,const char* name)     const;
  const char* getGroup        (OffsetBase offsetBase,const Property* prop) const;
  const char* getDocumentation(OffsetBase offsetBase,const char* name)     const;
  const char* getDocumentation(OffsetBase offsetBase,const Property* prop) const;

  Property *getPropertyByName(OffsetBase offsetBase,const char* name) const;
  void getPropertyMap(OffsetBase offsetBase,std::map<std::string,Property*> &Map) const;
  void getPropertyList(OffsetBase offsetBase,std::vector<Property*> &List) const;
  void getPropertyNamedList(OffsetBase offsetBase, std::vector<std::pair<const char*,Property*> > &List) const;
  /// See PropertyContainer::visitProperties for semantics
  void visitProperties(OffsetBase offsetBase, const std::function<void(Property*)>& visitor) const;

  void merge(PropertyData *other=nullptr) const;
  void split(PropertyData *other);
};


/** Base class of all classes with properties
 */
class AppExport PropertyContainer: public Base::Persistence
{

  TYPESYSTEM_HEADER_WITH_OVERRIDE();

public:
  /**
   * A constructor.
   * A more elaborate description of the constructor.
   */
  PropertyContainer();

  /**
   * A destructor.
   * A more elaborate description of the destructor.
   */
  ~PropertyContainer() override;

  unsigned int getMemSize () const override;

  virtual std::string getFullName() const {return {};}

  /// find a property by its name
  virtual Property *getPropertyByName(const char* name) const;
  /// get the name of a property
  virtual const char* getPropertyName(const Property* prop) const;
  /// get all properties of the class (including properties of the parent)
  virtual void getPropertyMap(std::map<std::string,Property*> &Map) const;
  /// get all properties of the class (including properties of the parent)
  virtual void getPropertyList(std::vector<Property*> &List) const;
  /// Call the given visitor for each property. The visiting order is undefined.
  /// This method is necessary because PropertyContainer has no begin and end methods
  /// and it is not practical to implement these.
  /// What gets visited is undefined if the collection of Properties is changed during this call.
  virtual void visitProperties(const std::function<void(Property*)>& visitor) const;
  /// get all properties with their names, may contain duplicates and aliases
  virtual void getPropertyNamedList(std::vector<std::pair<const char*,Property*> > &List) const;
  /// set the Status bit of all properties at once
  void setPropertyStatus(unsigned char bit,bool value);

  /// get the Type of a Property
  virtual short getPropertyType(const Property* prop) const;
  /// get the Type of a named Property
  virtual short getPropertyType(const char *name) const;
  /// get the Group of a Property
  virtual const char* getPropertyGroup(const Property* prop) const;
  /// get the Group of a named Property
  virtual const char* getPropertyGroup(const char *name) const;
  /// get the Group of a Property
  virtual const char* getPropertyDocumentation(const Property* prop) const;
  /// get the Group of a named Property
  virtual const char* getPropertyDocumentation(const char *name) const;
  /// check if the property is read-only
  bool isReadOnly(const Property* prop) const;
  /// check if the named property is read-only
  bool isReadOnly(const char *name) const;
  /// check if the property is hidden
  bool isHidden(const Property* prop) const;
  /// check if the named property is hidden
  bool isHidden(const char *name) const;
  virtual App::Property* addDynamicProperty(
        const char* type, const char* name=nullptr,
        const char* group=nullptr, const char* doc=nullptr,
        short attr=0, bool ro=false, bool hidden=false);

  DynamicProperty::PropData getDynamicPropertyData(const Property* prop) const {
      return dynamicProps.getDynamicPropertyData(prop);
  }

  bool changeDynamicProperty(const Property *prop, const char *group, const char *doc) {
      return dynamicProps.changeDynamicProperty(prop,group,doc);
  }

  virtual bool removeDynamicProperty(const char* name) {
      return dynamicProps.removeDynamicProperty(name);
  }
  virtual std::vector<std::string> getDynamicPropertyNames() const {
      return dynamicProps.getDynamicPropertyNames();
  }
  virtual App::Property *getDynamicPropertyByName(const char* name) const {
      return dynamicProps.getDynamicPropertyByName(name);
  }

  virtual void onPropertyStatusChanged(const Property &prop, unsigned long oldStatus);

  void Save (Base::Writer &writer) const override;
  void Restore(Base::XMLReader &reader) override;
  virtual void beforeSave() const;

  virtual void editProperty(const char * /*propName*/) {}

  const char *getPropertyPrefix() const {
      return _propertyPrefix.c_str();
  }

  void setPropertyPrefix(const char *prefix) {
      _propertyPrefix = prefix;
  }

  friend class Property;
  friend class DynamicProperty;


protected:
  /** get called by the container when a property has changed
   *
   * This function is called before onChanged()
   */
  virtual void onEarlyChange(const Property* /*prop*/){}
  /// get called by the container when a property has changed
  virtual void onChanged(const Property* /*prop*/){}
  /// get called before the value is changed
  virtual void onBeforeChange(const Property* /*prop*/){}

  //void hasChanged(Property* prop);
  static const  PropertyData * getPropertyDataPtr();
  virtual const PropertyData& getPropertyData() const;

  virtual void handleChangedPropertyName(Base::XMLReader &reader, const char * TypeName, const char *PropName);
  virtual void handleChangedPropertyType(Base::XMLReader &reader, const char * TypeName, Property * prop);

public:
  // forbidden
  PropertyContainer(const PropertyContainer&) = delete;
  PropertyContainer& operator = (const PropertyContainer&) = delete;

protected:
  DynamicProperty dynamicProps;

private:
  std::string _propertyPrefix;
  static PropertyData propertyData;
};

// clang-format off
/// Property define
#define _ADD_PROPERTY(_name,_prop_, _defaultval_) \
  do { \
    this->_prop_.setValue _defaultval_;\
    this->_prop_.setContainer(this); \
    propertyData.addProperty(static_cast<App::PropertyContainer*>(this), _name, &this->_prop_); \
  } while (0)

#define ADD_PROPERTY(_prop_, _defaultval_) \
    _ADD_PROPERTY(#_prop_, _prop_, _defaultval_)

#define _ADD_PROPERTY_TYPE(_name,_prop_, _defaultval_, _group_,_type_,_Docu_) \
  do { \
    this->_prop_.setValue _defaultval_;\
    this->_prop_.setContainer(this); \
    propertyData.addProperty(static_cast<App::PropertyContainer*>(this), _name, &this->_prop_, (_group_),(_type_),(_Docu_)); \
  } while (0)

#define ADD_PROPERTY_TYPE(_prop_, _defaultval_, _group_,_type_,_Docu_) \
    _ADD_PROPERTY_TYPE(#_prop_,_prop_,_defaultval_,_group_,_type_,_Docu_)


#define PROPERTY_HEADER(_class_) \
  TYPESYSTEM_HEADER(); \
protected: \
  static const App::PropertyData * getPropertyDataPtr(void); \
  virtual const App::PropertyData &getPropertyData(void) const; \
private: \
  static App::PropertyData propertyData

/// Like PROPERTY_HEADER, but with overridden methods declared as such
#define PROPERTY_HEADER_WITH_OVERRIDE(_class_) \
  TYPESYSTEM_HEADER_WITH_OVERRIDE(); \
public: \
  static consteval const char* getClassName() {\
    static_assert(sizeof(_class_) > 0, "Class is not complete"); \
    \
    constexpr const char* sClass = #_class_; \
    constexpr std::string_view vClass {sClass}; \
    static_assert(!vClass.empty(), "Class name must not be empty"); \
    static_assert(std::is_base_of<App::PropertyContainer, _class_>::value, \
                  "Class must be derived from App::PropertyContainer"); \
    \
    constexpr bool isSubClassOfDocObj = std::is_base_of<App::DocumentObject, _class_>::value && \
                                        !std::is_same<App::DocumentObject, _class_>::value; \
    if constexpr (isSubClassOfDocObj) { \
      constexpr auto pos = vClass.find("::"); \
      static_assert(pos != std::string_view::npos, \
                    "Class name must be fully qualified for document object derived classes"); \
      constexpr auto vNamespace = vClass.substr(0, pos); \
      static_assert(!vNamespace.empty(), "Namespace must not be empty"); \
      \
      constexpr std::string_view filePath = __FILE__; \
      constexpr std::string_view modPath = "/src/Mod/"; \
      constexpr auto posAfterSrcMod = filePath.find(modPath); \
      constexpr bool hasSrcModInPath = posAfterSrcMod != std::string_view::npos; \
      if constexpr (hasSrcModInPath) { \
        constexpr auto pathAfterSrcMod = filePath.substr(posAfterSrcMod + modPath.size()); \
        constexpr bool isPathOk = pathAfterSrcMod.starts_with(vNamespace); \
        static_assert( \
          /* some compilers evaluate static_asserts inside ifs before checking it's a valid codepath */ \
          !isSubClassOfDocObj || !hasSrcModInPath || \
          /* allow `Path` until it's been properly renamed to CAM */ \
          vNamespace == "Path" || isPathOk, \
          "Classes in `src/Mod` needs to be in a directory with the same name as" \
          " the namespace in order to load correctly"); \
      } \
    } \
    return sClass; \
  } \
protected: \
  static const App::PropertyData * getPropertyDataPtr(void); \
  const App::PropertyData &getPropertyData(void) const override; \
private: \
  static App::PropertyData propertyData
///
#define PROPERTY_SOURCE(_class_, _parentclass_) \
TYPESYSTEM_SOURCE_P(_class_)\
const App::PropertyData * _class_::getPropertyDataPtr(void){return &propertyData;} \
const App::PropertyData & _class_::getPropertyData(void) const{return propertyData;} \
App::PropertyData _class_::propertyData; \
void _class_::init(void){\
  initSubclass(_class_::classTypeId, #_class_ , #_parentclass_, &(_class_::create) ); \
  _class_::propertyData.parentPropertyData = _parentclass_::getPropertyDataPtr(); \
}

#define PROPERTY_SOURCE_ABSTRACT(_class_, _parentclass_) \
TYPESYSTEM_SOURCE_ABSTRACT_P(_class_)\
const App::PropertyData * _class_::getPropertyDataPtr(void){return &propertyData;} \
const App::PropertyData & _class_::getPropertyData(void) const{return propertyData;} \
App::PropertyData _class_::propertyData; \
void _class_::init(void){\
  initSubclass(_class_::classTypeId, #_class_ , #_parentclass_, &(_class_::create) ); \
  _class_::propertyData.parentPropertyData = _parentclass_::getPropertyDataPtr(); \
}

#define TYPESYSTEM_SOURCE_TEMPLATE(_class_) \
template<> Base::Type _class_::classTypeId = Base::Type::BadType; \
template<> Base::Type _class_::getClassTypeId(void) { return _class_::classTypeId; } \
template<> Base::Type _class_::getTypeId(void) const { return _class_::classTypeId; } \
template<> void * _class_::create(void){\
   return new _class_ ();\
}

#define PROPERTY_SOURCE_TEMPLATE(_class_, _parentclass_) \
TYPESYSTEM_SOURCE_TEMPLATE(_class_)\
template<> App::PropertyData _class_::propertyData = App::PropertyData(); \
template<> const App::PropertyData * _class_::getPropertyDataPtr(void){return &propertyData;} \
template<> const App::PropertyData & _class_::getPropertyData(void) const{return propertyData;} \
template<> void _class_::init(void){\
  initSubclass(_class_::classTypeId, #_class_ , #_parentclass_, &(_class_::create) ); \
  _class_::propertyData.parentPropertyData = _parentclass_::getPropertyDataPtr(); \
}
// clang-format on

} // namespace App

#endif // SRC_APP_PROPERTYCONTAINER_H_
