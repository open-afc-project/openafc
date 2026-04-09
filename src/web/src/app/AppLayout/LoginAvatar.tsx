import React from 'react';
import { useNavigate } from 'react-router-dom';
import { Button } from '@patternfly/react-core';
import { SignInAltIcon, SignOutAltIcon } from '@patternfly/react-icons';
import { isLoggedIn, UserContext, UserState } from '../Lib/User';
import { guiConfig } from '../Lib/RatApi';

const navLinkStyle: React.CSSProperties = {
  color: 'var(--pf-t--global--text--color--regular, #000)',
  textDecoration: 'none',
  padding: '0 8px',
  whiteSpace: 'nowrap',
};

const LoginButton: React.FunctionComponent = () => {
  const navigate = useNavigate();

  if (!guiConfig.about_url) {
    return (
      <Button variant="link" component="a" href={guiConfig.login_url} style={navLinkStyle} icon={<SignInAltIcon />} iconPosition="end">
        Login
      </Button>
    );
  } else {
    return (
      <Button variant="link" style={navLinkStyle} onClick={() => navigate('/login')} icon={<SignInAltIcon />} iconPosition="end">
        Login
      </Button>
    );
  }
};

export const LoginAvatar: React.FunctionComponent = () => {
  const showLogout = (
    <Button variant="link" component="a" href={guiConfig.logout_url} style={navLinkStyle} icon={<SignOutAltIcon />} iconPosition="end">
      Logout
    </Button>
  );

  return <UserContext.Consumer>{(user: UserState) => (isLoggedIn() ? showLogout : <LoginButton />)}</UserContext.Consumer>;
};
