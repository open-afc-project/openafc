import React from 'react';
import '@patternfly/react-core/dist/styles/base.css';
import { PageSection } from '@patternfly/react-core';
import { HashRouter } from 'react-router-dom';
import { AppLayout } from '@app/AppLayout/AppLayout';
import { AppRoutes } from '@app/routes';
import '@app/app.css';
import { logger } from './Lib/Logger';
// @ts-ignore
import { login, UserContext, configure, UserState, retrieveUserData } from './Lib/User';
import { clone } from './Lib/Utils';

class App extends React.Component<{ conf: Promise<void> }, { isReady: boolean; user: UserState }> {
  private sessionCheckInterval: any = null;

  constructor(props: Readonly<{ conf: Promise<void> }>) {
    super(props);
    logger.info('configuring user context');
    this.state = { isReady: false, user: { data: { loggedIn: false } } };
    configure(
      () => this.state.user,
      (x) => this.setState({ user: clone(x) }),
    );

    props.conf.then(async () => {
      logger.info('configuration loaded');
      await retrieveUserData();
      this.setState({ isReady: true });
    });
  }

  componentDidMount() {
    // Check session validity periodically (every 2 minutes) when logged in
    this.sessionCheckInterval = setInterval(() => {
      if (this.state.user.data.loggedIn) {
        retrieveUserData();
      }
    }, 120000);

    // Re-verify session when tab becomes visible or focused after being idle
    window.addEventListener('focus', this.handleTabActive);
    document.addEventListener('visibilitychange', this.handleTabActive);
  }

  componentWillUnmount() {
    if (this.sessionCheckInterval) {
      clearInterval(this.sessionCheckInterval);
    }
    window.removeEventListener('focus', this.handleTabActive);
    document.removeEventListener('visibilitychange', this.handleTabActive);
  }

  private handleTabActive = () => {
    if (document.visibilityState === 'visible' && this.state.user.data.loggedIn) {
      retrieveUserData();
    }
  };

  render() {
    return (
      <UserContext.Provider value={this.state.user}>
        <HashRouter>
          <AppLayout>{this.state.isReady ? <AppRoutes /> : <PageSection />}</AppLayout>
        </HashRouter>
      </UserContext.Provider>
    );
  }
}

export { App };
