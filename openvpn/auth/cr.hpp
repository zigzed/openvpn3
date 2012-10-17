//
//  cr.hpp
//  OpenVPN
//
//  Copyright (c) 2012 OpenVPN Technologies, Inc. All rights reserved.
//

#ifndef OPENVPN_AUTH_CR_H
#define OPENVPN_AUTH_CR_H

#include <string>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp> // for boost::algorithm::starts_with

#include <openvpn/common/exception.hpp>
#include <openvpn/common/base64.hpp>
#include <openvpn/common/split.hpp>
#include <openvpn/common/rc.hpp>

// Static Challenge response:
//   SCRV1:<BASE64_PASSWORD>:<BASE64_RESPONSE>
//
// Dynamic Challenge:
//   CRV1:<FLAGS>:<STATE_ID>:<BASE64_USERNAME>:<CHALLENGE_TEXT>
//   FLAGS is a comma-separated list of options:
//     E -- echo
//     R -- response required
//
// Dynamic Challenge response:
//   Username: [username decoded from username_base64]
//   Password: CRV1::<STATE_ID>::<RESPONSE_TEXT>

namespace openvpn {
  class ChallengeResponse : public RC<thread_unsafe_refcount> {
  public:
    typedef boost::intrusive_ptr<ChallengeResponse> Ptr;

    OPENVPN_SIMPLE_EXCEPTION(dynamic_challenge_parse_error);

    ChallengeResponse()
      : echo(false), response_required(false)
    {
    }

    explicit ChallengeResponse(const std::string& cookie)
      : echo(false), response_required(false)
    {
      init(cookie);
    }

    void init(const std::string& cookie)
    {
      typedef std::vector<std::string> StringList;
      StringList sl;
      sl.reserve(5);
      split_by_char_void<StringList, NullLex>(sl, cookie, ':', 4);
      if (sl.size() != 5)
	throw dynamic_challenge_parse_error();
      if (sl[0] != "CRV1")
	{
	  OPENVPN_LOG("*** DC no crv1");
	  throw dynamic_challenge_parse_error();
	}

      // parse options
      {
	StringList opt;
	opt.reserve(2);
	split_by_char_void<StringList, NullLex>(opt, sl[1], ',');
	for (StringList::const_iterator i = opt.begin(); i != opt.end(); ++i)
	  {
	    if (*i == "E")
	      echo = true;
	    else if (*i == "R")
	      response_required = true;
	  }
      }

      // save state ID
      state_id = sl[2];

      // save username
      try {
	username = base64->decode(sl[3]);
      }
      catch (const Base64::base64_decode_error&)
	{
	  throw dynamic_challenge_parse_error();
	}

      // save challenge
      challenge_text = sl[4];
    }

    static bool is_dynamic(const std::string& s)
    {
      return boost::algorithm::starts_with(s, "CRV1:");
    }

    static void validate_dynamic(const std::string& cookie)
    {
      ChallengeResponse cr(cookie);
    }

    std::string construct_dynamic_password(const std::string& response) const
    {
      std::ostringstream os;
      os << "CRV1::" << state_id << "::" << response;
      return os.str();
    }

    static std::string construct_static_password(const std::string& password,
						 const std::string& response)
    {
      std::ostringstream os;
      os << "SCRV1:" << base64->encode(password) << ':' << base64->encode(response);
      return os.str();
    }

    const std::string& get_state_id() const { return state_id; }
    const std::string& get_username() const { return username; }
    bool get_echo() const { return echo; }
    bool get_response_required() const { return response_required; }
    const std::string& get_challenge_text() const { return challenge_text; }

  private:
    bool echo;
    bool response_required;
    std::string state_id;
    std::string username;
    std::string challenge_text;
  };
}

#endif