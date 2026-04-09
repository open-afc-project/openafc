import React from 'react';
import {
  Title,
  Card,
  CardBody,
  PageSection,
  Button,
  Alert,
  AlertActionCloseButton,
} from '@patternfly/react-core';
import { Navigate } from 'react-router-dom';
import { guiConfig, getAboutLoginAfc } from '../Lib/RatApi';
import { isLoggedIn, UserContext, UserState } from '../Lib/User';

class AppLoginPage extends React.Component<any, { content: string; messageType?: string; messageValue?: string }> {
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
    return (
      <UserContext.Consumer>
        {(u: UserState) =>
          isLoggedIn() ? (
            <Navigate to="/dashboard" replace />
          ) : (
            <PageSection>
              <div>
                <Title headingLevel="h2">AFC Login</Title>
                <Card>
                  <CardBody dangerouslySetInnerHTML={{ __html: this.state.content }} />
                </Card>
                <>
                  {this.state.messageType && (
                    <Alert
                      variant={this.state.messageType as any}
                      title={this.state.messageValue || ''}
                      actionClose={<AlertActionCloseButton onClose={this.hideAlert} />}
                    />
                  )}
                </>
              </div>
            </PageSection>
          )
        }
      </UserContext.Consumer>
    );
  }
}

export default AppLoginPage;
