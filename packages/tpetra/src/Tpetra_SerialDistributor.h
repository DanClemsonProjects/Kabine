/*Paul
18-Nov-2002 Copied from Epetra_SerialDistributor.h
19-Nov-2002 Templated for <PT,OT>, modified slightly.
23-Nov-2002 Default constructor modified, argument commented out
            do methods temporarily renamed to Do
25-Nov-2002 do methods fixed.
*/

#ifndef _TPETRA_SERIALDISTRIBUTOR_H_
#define _TPETRA_SERIALDISTRIBUTOR_H_

#include "Tpetra_Object.h"
#include "Tpetra_Distributor.h"

namespace Tpetra {

//! Tpetra::SerialDistributor:  The Tpetra Serial implementation of the Tpetra::Distributor Gather/Scatter Setup Class.
/*! The SerialDistributor class is an Serial implement of Tpetra::Distributor that is essentially a trivial class
    since a serial machine is a trivial parallel machine.
		An SerialDistributor object is actually produced by calling a method in the Tpetra::SerialPlatform class.

		Most SerialDistributor methods throw an error of -1, since they should never be called.
*/

	template<typename PacketType, typename OrdinalType>
	class SerialDistributor : public Object, public virtual Distributor<PacketType, OrdinalType> {
  public:

  //@{ \name Constructor/Destructor

  //! Default Constructor.
  SerialDistributor() : Object("Tpetra::Distributor[Serial]") {};

  //! Copy Constructor
  SerialDistributor(const SerialDistributor<PacketType, OrdinalType>& Plan) : Object(Plan.label()) {};

  //! Clone method
	Distributor<PacketType, OrdinalType>* clone() {
		Distributor<PacketType, OrdinalType>* distributor = static_cast<Distributor<PacketType, OrdinalType>*>
			(new SerialDistributor<PacketType, OrdinalType>(*this)); 
		return(distributor); 
	};

  //! Destructor.
  virtual ~SerialDistributor() {};
  //@}

	//@{ \name Gather/Scatter Constructors
  //! Create Distributor object using list of Image IDs to send to
  void createFromSends(const OrdinalType& numExportIDs, const OrdinalType* exportImageIDs,
											 const bool& deterministic, OrdinalType& numRemoteIDs ) 
		{throw reportError("This method should never be called.", -1);};
	//! Create Distributor object using list of Image IDs to receive from
	void createFromRecvs(const OrdinalType& numRemoteIDs, const OrdinalType* remoteGIDs, 
											 const OrdinalType* remoteImageIDs, const bool& deterministic, 
											 OrdinalType& numExportIDs, OrdinalType*& exportGIDs, 
											 OrdinalType*& exportImageIDs)
		{throw reportError("This method should never be called.", -1);};
	//@}
	
	//@{ \name Constant Size
	//! do
  void doPostsAndWaits       (PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs) 
		{throw reportError("This method should never be called.", -1);};
	//! doReverse
  void doReversePostsAndWaits(PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};

	//! doPosts
  void doPosts(PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//! doWaits
  void doWaits(PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};

	//! doReversePosts
  void doReversePosts(PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//! doReverseWaits
  void doReverseWaits(PacketType* export_objs, const OrdinalType& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//@}

	//@{ \name Non-Constant Size
	//! do
  void doPostsAndWaits       (PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//! doReverse
  void doReversePostsAndWaits(PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};

	//! doPosts
  void doPosts(PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//! doWaits
  void doWaits(PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};

	//! doReversePosts
  void doReversePosts(PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//! doReverseWaits
  void doReverseWaits(PacketType* export_objs, const OrdinalType*& obj_size, PacketType* import_objs)
		{throw reportError("This method should never be called.", -1);};
	//@}

	//@{ \name I/O Methods
	//! print method inherited from Object
  void print(ostream& os) const {os << label();};
	//! printInfo method inherited from Distributor
  void printInfo(ostream& os) const {print(os);};
	//@}

}; // class SerialDistributor

} // namespace Tpetra

#endif // _TPETRA_SERIALDISTRIBUTOR_H_
