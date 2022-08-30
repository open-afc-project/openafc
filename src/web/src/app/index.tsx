import * as React from "react";
import "@patternfly/react-core/dist/styles/base.css";
import { PageSection } from "@patternfly/react-core";
import { HashRouter } from "react-router-dom";
import { AppLayout } from "@app/AppLayout/AppLayout";
import { AppRoutes } from "@app/routes";
import "@app/app.css";
import { logger } from "./Lib/Logger";
import { login, UserContext, configure, UserState, retrieveUserData} from "./Lib/User";
import { clone } from "./Lib/Utils";

// index.tsx: definition of app component
// author: Sam Smucny

class App extends React.Component<{ conf: Promise<void> }, { isReady: boolean, user: UserState }> {

  constructor(props: Readonly<{ conf: Promise<void> }>) {
    super(props);
    logger.info("configuring user context");
    this.state = { isReady: false, user: { data: { loggedIn: false } } };
    configure(() => this.state.user, (x) => this.setState({ user: clone(x) }));

    // when the configuration promise resolves then set the app config and render the app
    props.conf.then(async x => {
      logger.info("configuration loaded")
      await retrieveUserData();
      this.setState({ isReady: true })
    });
  }

  render() {
      return (
        // wrap application in user provider to give components access to login info
        <UserContext.Provider value={this.state.user}>
          <HashRouter>
            <AppLayout>
              {this.state.isReady ? <AppRoutes /> :  <PageSection/>}
            </AppLayout>
          </HashRouter>
        </UserContext.Provider> 
      );
  }
}

export { App };
