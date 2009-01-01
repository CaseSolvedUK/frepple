/***************************************************************************
  file : $URL$
  version : $LastChangedRevision$  $LastChangedBy$
  date : $LastChangedDate$
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 * Copyright (C) 2007 by Johan De Taeye                                    *
 *                                                                         *
 * This library is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU Lesser General Public License as published   *
 * by the Free Software Foundation; either version 2.1 of the License, or  *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This library is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser *
 * General Public License for more details.                                *
 *                                                                         *
 * You should have received a copy of the GNU Lesser General Public        *
 * License along with this library; if not, write to the Free Software     *
 * Foundation Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA *
 *                                                                         *
 ***************************************************************************/


#define FREPPLE_CORE
#include "frepple/model.h"
namespace frepple
{


DECLARE_EXPORT void Load::validate(Action action)
{
  // Catch null operation and resource pointers
  Operation *oper = getOperation();
  Resource *res = getResource();
  if (!oper || !res)
  {
    // Invalid load model
    delete this;
    if (!oper && !res)
      throw DataException("Missing operation and resource on a load");
    else if (!oper)
      throw DataException("Missing operation on a load on resource '"
          + res->getName() + "'");
    else if (!res)
      throw DataException("Missing resource on a load on operation '"
          + oper->getName() + "'");
  }

  // Check if a load with 1) identical resource, 2) identical operation and 
  // 3) overlapping effectivity dates already exists
  Operation::loadlist::const_iterator i = oper->getLoads().begin();
  for (;i != oper->getLoads().end(); ++i)
    if (i->getResource() == res 
      && i->getEffective().overlap(getEffective()) 
      && &*i != this) 
        break;

  // Apply the appropriate action
  switch (action)
  {
    case ADD:
      if (i != oper->getLoads().end())
      {
        delete this;
        throw DataException("Load of '" + oper->getName() + "' and '"
            + res->getName() + "' already exists");
      }
      break;
    case CHANGE:
      delete this;
      throw DataException("Can't update a load");
    case ADD_CHANGE:
      // ADD is handled in the code after the switch statement
      if (i == oper->getLoads().end()) break;
      delete this;
      throw DataException("Can't update a load");
    case REMOVE:
      // This load was only used temporarily during the reading process
      delete this;
      if (i == oper->getLoads().end())
        // Nothing to delete
        throw DataException("Can't remove nonexistent load of '"
            + oper->getName() + "' and '" + res->getName() + "'");
      delete &*i;
      // Set a flag to make sure the level computation is triggered again
      HasLevel::triggerLazyRecomputation();
      return;
  }

  // The statements below should be executed only when a new load is created.

  // If the resource has an owner, also load the owner
  // Note that the owner load can create more loads if it has an owner too.
  if (res->hasOwner() && action!=REMOVE) new Load(oper, res->getOwner(), qty);

  // Set a flag to make sure the level computation is triggered again
  HasLevel::triggerLazyRecomputation();
}


DECLARE_EXPORT Load::~Load()
{
  // Set a flag to make sure the level computation is triggered again
  HasLevel::triggerLazyRecomputation();

  // Delete existing loadplans
  if (getOperation() && getResource())
  {
    // Loop over operationplans
	  for(OperationPlan::iterator i(getOperation()); i != OperationPlan::end(); ++i)
      // Loop over loadplans
      for(OperationPlan::LoadPlanIterator j = i->beginLoadPlans(); j != i->endLoadPlans(); )
        if (j->getLoad() == this) j.deleteLoadPlan();
        else ++j;
  }

  // Delete the load from the operation and resource
  if (getOperation()) getOperation()->loaddata.erase(this);
  if (getResource()) getResource()->loads.erase(this);
}


DECLARE_EXPORT void Load::writeElement(XMLOutput *o, const Keyword& tag, mode m) const
{
  // If the load has already been saved, no need to repeat it again
  // A 'reference' to a load is not useful to be saved.
  if (m == REFERENCE) return;
  assert(m != NOHEADER);

  o->BeginObject(tag);

  // If the load is defined inside of an operation tag, we don't need to save
  // the operation. Otherwise we do save it...
  if (!dynamic_cast<Operation*>(o->getPreviousObject()))
    o->writeElement(Tags::tag_operation, getOperation());

  // If the load is defined inside of an resource tag, we don't need to save
  // the resource. Otherwise we do save it...
  if (!dynamic_cast<Resource*>(o->getPreviousObject()))
    o->writeElement(Tags::tag_resource, getResource());

  // Write the quantity
  if (qty != 1.0) o->writeElement(Tags::tag_quantity, qty);

  // Write the effective daterange
  if (getEffective().getStart() != Date::infinitePast)
    o->writeElement(Tags::tag_effective_start, getEffective().getStart());
  if (getEffective().getEnd() != Date::infiniteFuture)
    o->writeElement(Tags::tag_effective_end, getEffective().getEnd());

  o->EndObject(tag);
}


DECLARE_EXPORT void Load::beginElement (XMLInput& pIn, const Attribute& pAttr)
{
  if (pAttr.isA (Tags::tag_resource))
    pIn.readto( Resource::reader(Resource::metadata,pIn.getAttributes()) );
  else if (pAttr.isA (Tags::tag_operation))
    pIn.readto( Operation::reader(Operation::metadata,pIn.getAttributes()) );
}


DECLARE_EXPORT void Load::endElement (XMLInput& pIn, const Attribute& pAttr, const DataElement& pElement)
{
  if (pAttr.isA (Tags::tag_resource))
  {
    Resource * r = dynamic_cast<Resource*>(pIn.getPreviousObject());
    if (r) setResource(r);
    else throw LogicException("Incorrect object type during read operation");
  }
  else if (pAttr.isA (Tags::tag_operation))
  {
    Operation * o = dynamic_cast<Operation*>(pIn.getPreviousObject());
    if (o) setOperation(o);
    else throw LogicException("Incorrect object type during read operation");
  }
  else if (pAttr.isA(Tags::tag_quantity))
    setQuantity(pElement.getDouble());
  else if (pAttr.isA(Tags::tag_action))
  {
    delete static_cast<Action*>(pIn.getUserArea());
    pIn.setUserArea(
      new Action(MetaClass::decodeAction(pElement.getString().c_str()))
    );
  }
  else if (pAttr.isA(Tags::tag_effective_end))
    setEffectiveEnd(pElement.getDate());
  else if (pAttr.isA(Tags::tag_effective_start))
    setEffectiveStart(pElement.getDate());
  else if (pIn.isObjectEnd())
  {
    // The load data is now all read in. See if it makes sense now...
    validate(!pIn.getUserArea() ?
             ADD_CHANGE :
             *static_cast<Action*>(pIn.getUserArea())
             );
    delete static_cast<Action*>(pIn.getUserArea());
  }
}


int PythonLoad::initialize(PyObject* m)
{
  // Initialize the type
  PythonType& x = getType();
  x.setName("load");
  x.setDoc("frePPLe load");
  x.supportgetattro();
  x.supportsetattro();
  const_cast<MetaCategory&>(Load::metadata).factoryPythonProxy = proxy;
  return x.typeReady(m);
}


DECLARE_EXPORT PyObject* PythonLoad::getattro(const Attribute& attr)
{
  if (!ld) return Py_BuildValue("");
  if (attr.isA(Tags::tag_resource))
    return PythonObject(ld->getResource());
  if (attr.isA(Tags::tag_operation))
    return PythonObject(ld->getOperation());
  if (attr.isA(Tags::tag_quantity))
    return PythonObject(ld->getQuantity());
  if (attr.isA(Tags::tag_effective_end))
    return PythonObject(ld->getEffective().getEnd());
  if (attr.isA(Tags::tag_effective_start))
    return PythonObject(ld->getEffective().getStart());
  return NULL;
}


DECLARE_EXPORT int PythonLoad::setattro(const Attribute& attr, const PythonObject& field)
{
  if (attr.isA(Tags::tag_resource))
  {
    if (!field.check(PythonResource::getType())) 
    {
      PyErr_SetString(PythonDataException, "load resource must be of type resource");
      return -1;
    }
    Resource* y = static_cast<PythonResource*>(static_cast<PyObject*>(field))->obj;
    ld->setResource(y);
  }
  else if (attr.isA(Tags::tag_operation))
  {
    if (!field.check(PythonOperation::getType())) 
    {
      PyErr_SetString(PythonDataException, "load operation must be of type operation");
      return -1;
    }
    Operation* y = static_cast<PythonOperation*>(static_cast<PyObject*>(field))->obj;
    ld->setOperation(y);
  }
  else if (attr.isA(Tags::tag_quantity))
    ld->setQuantity(field.getDouble());
  else if (attr.isA(Tags::tag_effective_end))
    ld->setEffectiveEnd(field.getDate());
  else if (attr.isA(Tags::tag_effective_start))
    ld->setEffectiveStart(field.getDate());
  else
    return -1;
  return 0;
}


int PythonLoadIterator::initialize(PyObject* m)
{
  // Initialize the type
  PythonType& x = PythonExtension<PythonLoadIterator>::getType();
  x.setName("loadIterator");
  x.setDoc("frePPLe iterator for loads");
  x.supportiter();
  return x.typeReady(m);
}


PyObject* PythonLoadIterator::iternext()
{  
  if (res) 
  {
    // Iterate over loads on a resource 
    if (ir == res->getLoads().end()) return NULL;
    return PythonObject(const_cast<Load*>(&*(ir++)));
  }
  else
  {
    // Iterate over loads on an operation 
    if (io == oper->getLoads().end()) return NULL;
    return PythonObject(const_cast<Load*>(&*(io++)));
  }
}

} // end namespace
