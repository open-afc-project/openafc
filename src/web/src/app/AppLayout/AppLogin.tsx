import React from 'react';
import * as React from 'react';
import {
  Title,
  Card,
  CardHead,
  CardHeader,
  CardBody,
  PageSection,
  InputGroup,
  TextInput,
  FormGroup,
  Button,
  Alert,
  AlertActionCloseButton,
} from '@patternfly/react-core';
import { guiConfig, getAboutLoginAfc } from '../Lib/RatApi';

/**
 * AppLogin.ts: Login page for web site
 * author: patternfly, Sam Smucny
 */

// Currently there is no logo/background on app pages, so if we want to brand the site this is one place to do it
// Note: When using background-filter.svg, you must also include #image_overlay as the fragment identifier

/**
 * Application login page
 */
class AppLoginPage extends React.Component {
  constructor(props: any) {
    super(props);
    this.state = { content: '' };
    getAboutLoginAfc().then((res) =>
      res.kind === 'Success'
        ? this.setState({ content: res.result })
        : this.setState({
            messageType: 'danger',
            messageValue: res.description,
          }),
    );
  }

  private hideAlert = () => this.setState({ messageType: undefined });

  render() {
    // the commented lines are not being used. If we want to add functionality for them at a later date then uncomment
    return (
      <PageSection>
        <div>
          <Title size={'lg'}>AFC Login</Title>
          <Card>
            {' '}
            <CardBody dangerouslySetInnerHTML={{ __html: this.state.content }} />
          </Card>
          <>
            {this.state.messageType && (
              <Alert
                variant={this.state.messageType}
                title={this.state.messageValue}
                action={<AlertActionCloseButton onClose={this.hideAlert} />}
              />
            )}
          </>
        </div>
      </PageSection>
    );
  }
}

export default AppLoginPage;
