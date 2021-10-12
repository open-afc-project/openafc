import React from "react";
import {
  LoginFooterItem,
  LoginForm,
  LoginMainFooterBandItem,
  LoginMainFooterLinksItem,
  LoginPage,
  BackgroundImageSrc,
  ListItem,
  Checkbox
} from "@patternfly/react-core";
import { ExclamationCircleIcon, UserAltIcon } from "@patternfly/react-icons";
import { logger } from "../Lib/Logger";
import { login } from "../Lib/User";
import { guiConfig } from "../Lib/RatApi";

/**
 * AppLogin.ts: Login page for web site
 * author: patternfly, Sam Smucny
 */

// Currently there is no logo/background on app pages, so if we want to brand the site this is one place to do it
// Note: When using background-filter.svg, you must also include #image_overlay as the fragment identifier

/**
 * Interface definition of `Login` state object
 */
interface LoginState {
  showHelperText: boolean,
  usernameValue: string,
  isValidUsername: boolean,
  passwordValue: string,
  isValidPassword: boolean,
  isRememberMeChecked: boolean,
  errormessage?: string
}

/**
 * Application login page
 */
class AppLoginPage extends React.Component<{}, LoginState> {
  constructor(props: any) {
    super(props);
    this.state = {
      showHelperText: false,
      usernameValue: "",
      isValidUsername: true,
      passwordValue: "",
      isValidPassword: true,
      isRememberMeChecked: false
    };
  }

  handleUsernameChange = (value: string) => {
    this.setState({ usernameValue: value });
  }

  handlePasswordChange = (passwordValue: string) => {
    this.setState({ passwordValue });
  }

  onRememberMeClick = (checked: boolean) => {
    logger.info("old remember me: ", this.state.isRememberMeChecked);
    this.setState({ isRememberMeChecked: checked });
    logger.info("new remember me: ", this.state.isRememberMeChecked);
  }

  onLoginButtonClick = (event: any) => {
    event.preventDefault();
    this.setState({ isValidUsername: !!this.state.usernameValue });
    this.setState({ isValidPassword: !!this.state.passwordValue });
    this.setState({ showHelperText: !this.state.usernameValue || !this.state.passwordValue });
    
    if (!this.state.usernameValue || !this.state.passwordValue) return;

    // do the login
    login(this.state.usernameValue, this.state.passwordValue, this.state.isRememberMeChecked)
    .then(result => {
      if (result.kind === "Success") {
        logger.info("login successful")

        // redirect back to home if successful
        window.location.hash = "#"
      } else {
        logger.error(result);
        this.setState({ showHelperText: true, errormessage: " Invalid login credentials" });
      }
    })
    .catch(err => {
      logger.error(err);
      this.setState({ errormessage: err.toString(), showHelperText: true })
    })
  }

  render() {
    const helperText = (
      <React.Fragment>
        <ExclamationCircleIcon />
        {this.state.errormessage}
      </React.Fragment>
    );

    const signUpForAccountMessage = (
      <LoginMainFooterBandItem>
        Need an account? <a href={guiConfig.user_url}>Sign up.</a>
      </LoginMainFooterBandItem>
    );
    const forgotCredentials = (
      <LoginMainFooterBandItem>
        <a href={guiConfig.user_url.replace("register", "forgot-password")}>Forgot username or password?</a>
      </LoginMainFooterBandItem>
    );

    // These don't link anywhere. If we have legal pages they would go here
    const listItem = (
      <React.Fragment>
        <ListItem>
          <LoginFooterItem href="#">Terms of Use </LoginFooterItem>
        </ListItem>
        <ListItem>
          <LoginFooterItem href="#">Help</LoginFooterItem>
        </ListItem>
        <ListItem>
          <LoginFooterItem href="#">Privacy Policy</LoginFooterItem>
        </ListItem>
      </React.Fragment>
    );

    const loginForm = (
      <LoginForm
        showHelperText={this.state.showHelperText}
        helperText={helperText}
        usernameLabel="Email"
        usernameValue={this.state.usernameValue}
        onChangeUsername={this.handleUsernameChange}
        isValidUsername={this.state.isValidUsername}
        passwordLabel="Password"
        passwordValue={this.state.passwordValue}
        onChangePassword={this.handlePasswordChange}
        isValidPassword={this.state.isValidPassword}
        rememberMeLabel=""
        onLoginButtonClick={this.onLoginButtonClick}
      />
    );

    // the commented lines are not being used. If we want to add functionality for them at a later date then uncomment
    return (
      <LoginPage
        footerListVariants="inline"
        // brandImgSrc={brandImg}
        // brandImgAlt="PatternFly logo"
        // backgroundImgSrc={images}
        backgroundImgAlt="Images"
        // footerListItems={listItem}
        // textContent="This is placeholder text only. Use this area to place any information or introductory message about your
        // application that may be relevant to users."
        loginTitle="Log in to your account"
        // loginSubtitle="Please use your single sign-on LDAP credentials"
        // socialMediaLoginContent={socialMediaLoginContent}
        signUpForAccountMessage={signUpForAccountMessage}
        forgotCredentials={forgotCredentials}
      >
        <Checkbox
          id="pf-login-remember-me-id"
          label={"Keep me logged in for 30 days."}
          isChecked={this.state.isRememberMeChecked}
          onChange={this.onRememberMeClick}
        />
        {loginForm}
      </LoginPage>
    );
  }
}

export default AppLoginPage;
