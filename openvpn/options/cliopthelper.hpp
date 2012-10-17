//
//  cliopthelper.hpp
//  OpenVPN
//
//  Copyright (c) 2012 OpenVPN Technologies, Inc. All rights reserved.
//

#ifndef OPENVPN_OPTIONS_CLIOPTHELPER_H
#define OPENVPN_OPTIONS_CLIOPTHELPER_H

#include <vector>
#include <string>
#include <sstream>

#include <openvpn/common/exception.hpp>
#include <openvpn/common/options.hpp>
#include <openvpn/common/string.hpp>
#include <openvpn/options/remotelist.hpp>

namespace openvpn {
  class ParseClientConfig {
  public:
    struct ServerEntry {
      std::string server;
      std::string friendlyName;
    };

    struct ServerList : public std::vector<ServerEntry>
    {
    };

    ParseClientConfig(const OptionList& options)
    {
      try {
	// reset POD types
	reset_pod();

	// server-locked profiles not supported
	{
	  const OptionList::IndexList* se = options.get_index_ptr("setenv");
	  if (se)
	    {
	      for (OptionList::IndexList::const_iterator i = se->begin(); i != se->end(); ++i)
		{
		  const Option& o = options[*i];
		  const std::string arg1 = o.get_optional(1);
		  if (arg1 == "GENERIC_CONFIG")
		    {
		      error_ = true;
		      message_ = "SERVER_LOCKED_UNSUPPORTED: server locked profiles are currently unsupported";
		      return;
		    }
		}
	    }
	}

	// userlocked username
	{
	  const Option* o = options.get_ptr("USERNAME");
	  if (o)
	    userlockedUsername_ = o->get(1);
	}

	// External PKI
	externalPki_ = is_external_pki(options);

	// autologin
	autologin_ = is_autologin(options);

	// static challenge
	{
	  const Option* o = options.get_ptr("static-challenge");
	  if (o)
	    {
	      staticChallenge_ = o->get(1);
	      if (o->get_optional(2) == "1")
		staticChallengeEcho_ = true;
	    }
	}

	// profile name
	{
	  const Option* o = options.get_ptr("PROFILE");
	  if (o)
	    {
	      // take PROFILE substring up to '/'
	      const std::string& pn = o->get(1);
	      const size_t slashpos = pn.find('/');
	      if (slashpos != std::string::npos)
		profileName_ = pn.substr(0, slashpos);
	      else
		profileName_ = pn;
	    }
	  else
	    {
	      RemoteList rl(options);
	      if (rl.size() >= 1)
		profileName_ = rl[0].server_host;
	    }
	}

	// friendly name
	{
	  const Option* o = options.get_ptr("FRIENDLY_NAME");
	  if (o)
	    friendlyName_ = o->get(1);
	}

	// server list
	{
	  const Option* o = options.get_ptr("HOST_LIST");
	  if (o)
	    {
	      std::stringstream in(o->get(1));
	      std::string line;
	      while (std::getline(in, line))
		{
		  ServerEntry se;
		  se.server = line;
		  se.friendlyName = se.server;
		  serverList_.push_back(se);
		}
	    }
	}
      }
      catch (const std::exception& e)
	{
	  error_ = true;
	  message_ = e.what();
	}
    }

    static ParseClientConfig parse(const std::string& content)
    {
      OptionList options;
      return parse(content, options);
    }

    static ParseClientConfig parse(const std::string& content, OptionList& options)
    {
      try {
	options.clear();
	options.parse_from_config(content);
	options.parse_meta_from_config(content, "OVPN_ACCESS_SERVER");
	options.update_map();
	return ParseClientConfig(options);
      }
      catch (const std::exception& e)
	{
	  ParseClientConfig ret;
	  ret.error_ = true;
	  ret.message_ = e.what();
          return ret;
	}
    }

    // true if error
    bool error() const { return error_; }

    // if error, message given here
    const std::string& message() const { return message_; }

    // this username must be used with profile
    const std::string& userlockedUsername() const { return userlockedUsername_; }

    // profile name of config
    const std::string& profileName() const { return profileName_; }

    // "friendly" name of config
    const std::string& friendlyName() const { return friendlyName_; }

    // true: no creds required, false: username/password required
    bool autologin() const { return autologin_; }

    // if true, this is an External PKI profile (no cert or key directives)
    bool externalPki() const { return externalPki_; }

    // static challenge, may be empty, ignored if autologin
    const std::string& staticChallenge() const { return staticChallenge_; }

    // true if static challenge response should be echoed to UI, ignored if autologin
    bool staticChallengeEcho() const { return staticChallengeEcho_; }

    // optional list of user-selectable VPN servers
    const ServerList& serverList() const { return serverList_; }

    std::string to_string() const
    {
      std::ostringstream os;
      os << "user=" << userlockedUsername_
	 << " pn=" << profileName_
	 << " fn=" << friendlyName_
	 << " auto=" << autologin_
	 << " epki=" << externalPki_
	 << " schal=" << staticChallenge_
	 << " scecho=" << staticChallengeEcho_;
      return os.str();
    }

    static bool is_external_pki(const OptionList& options)
    {
      const Option* epki = options.get_ptr("EXTERNAL_PKI");
      if (epki)
	return string::is_true(epki->get_optional(1));
      else
	{
	  const Option* cert = options.get_ptr("cert");
	  const Option* key = options.get_ptr("key");
	  return !cert || !key;
	}
    }

    static bool is_autologin(const OptionList& options)
    {
      const Option* autologin = options.get_ptr("AUTOLOGIN");
      if (autologin)
	return string::is_true(autologin->get_optional(1));
      else
	{
	  const Option* auth_user_pass = options.get_ptr("auth-user-pass");
	  bool ret = !auth_user_pass;
	  if (ret)
	    {
	      // External PKI profiles from AS don't declare auth-user-pass,
	      // and we have no way of knowing if they are autologin unless
	      // we examine their cert, which requires accessing the system-level
	      // cert store on the client.  For now, we are going to assume
	      // that External PKI profiles from the AS are always userlogin,
	      // unless explicitly overriden by AUTOLOGIN above.
	      if (is_external_pki(options))
		return false;
	    }
	  return ret;
	}
    }

  private:
    ParseClientConfig()
    {
      reset_pod();
    }

    void reset_pod()
    {
      error_ = autologin_ = externalPki_ = staticChallengeEcho_ = false;
    }

    bool error_;
    std::string message_;
    std::string userlockedUsername_;
    std::string profileName_;
    std::string friendlyName_;
    bool autologin_;
    bool externalPki_;
    std::string staticChallenge_;
    bool staticChallengeEcho_;
    ServerList serverList_;
  };
}

#endif