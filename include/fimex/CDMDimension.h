#ifndef CDMDIMENSION_H_
#define CDMDIMENSION_H_

#include <string>
#include <ostream>

namespace MetNoFimex
{

class CDMDimension
{
public:
	CDMDimension(); // default null constructor for maps
	CDMDimension(std::string name, long length);
	virtual ~CDMDimension();
	const std::string& getName() const {return name;}
	size_t getLength() const {return length;}
	void setLength(size_t length) {this->length = length;}
	void setUnlimited(int unlimited) {this->unlimited = unlimited;}
	int isUnlimited() const {return unlimited;}
	/**
	 *  @brief print xml representation to stream
	 * 
	 * @param out stream to write xml to
	 */
	void toXMLStream(std::ostream& out) const;
private:
	std::string name;
	size_t length;
	int unlimited;

};

}

#endif /*CDMDIMENSION_H_*/
