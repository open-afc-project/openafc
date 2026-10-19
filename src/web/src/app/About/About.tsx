import React from 'react';
import {
  Title,
  Card,
  CardBody,
  PageSection,
  FormGroup,
  Button,
  Alert,
  AlertActionCloseButton,
  TextInput,
} from '@patternfly/react-core';
import { logger } from '../Lib/Logger';
// @ts-ignore
import ReCAPTCHA from 'react-google-recaptcha';
import { setAboutAfc } from '../Lib/RatApi';
import { PlusCircleIcon } from '@patternfly/react-icons';

export class About extends React.Component<any, any> {
  constructor(props: Readonly<{ content: any; sitekey: string }>) {
    super(props);
    this.state = {
      content: props.content.result,
      name: '',
      email: '',
      org: '',
      token: '',
      sitekey: props.sitekey,
      recaptchaError: false,
    };
    logger.info('ABOUT state: ' + this.state.content);
  }

  onChange = (value: any) => {
    this.setState({ token: value });
  };

  private submit() {
    if (!this.state.token && this.state.sitekey) {
      this.setState({ messageType: 'danger', messageValue: 'Captcha completion required ' });
      return;
    }
    if (!this.state.name || !this.state.email || !this.state.org) {
      this.setState({ messageType: 'danger', messageValue: 'User information is required' });
      return;
    }

    setAboutAfc(this.state.name, this.state.email, this.state.org, this.state.token).then((res) => {
      if (res.kind === 'Success') {
        this.setState({
          messageType: 'success',
          messageValue: 'Request for ' + this.state.email + ' has been submitted',
        });
      } else {
        this.setState({ messageType: 'danger', messageValue: res.description });
      }
    });
  }

  private hideAlert = () => this.setState({ messageType: undefined });

  render() {
    const nameChange = (_event: any, s?: string) => this.setState({ name: s });
    const emailChange = (_event: any, s?: string) => this.setState({ email: s });
    const orgChange = (_event: any, s?: string) => this.setState({ org: s });

    return (
      <PageSection>
        <div>
          <Title headingLevel="h2">Request Access to the AFC Website</Title>
          <Card>
            <CardBody dangerouslySetInnerHTML={{ __html: this.state.content }} />
          </Card>
          <Card>
            <CardBody>
              <FormGroup label="Full Name" isRequired={true} fieldId="user-full-name">
                <TextInput
                  type="text"
                  id="user-full-name"
                  name="user-full-name"
                  value={this.state.name}
                  onChange={nameChange}
                />
              </FormGroup>
            </CardBody>
          </Card>
          <Card>
            <CardBody>
              <FormGroup label="Email" isRequired={true} fieldId="user-email">
                <TextInput
                  type="text"
                  id="user-email"
                  name="user-email"
                  value={this.state.email}
                  onChange={emailChange}
                />
              </FormGroup>
            </CardBody>
          </Card>
          <Card>
            <CardBody>
              <FormGroup label="Organization" isRequired={true} fieldId="user-org">
                <TextInput type="text" id="user-org" name="user-org" value={this.state.org} onChange={orgChange} />
              </FormGroup>
            </CardBody>
          </Card>
          <>
            {this.state.messageType && (
              <Alert
                variant={this.state.messageType}
                title={this.state.messageValue}
                actionClose={<AlertActionCloseButton onClose={this.hideAlert} />}
              />
            )}
          </>
          <br />
          <Button variant="primary" icon={<PlusCircleIcon />} onClick={() => this.submit()}>
            Submit
          </Button>
          {this.state.sitekey && !this.state.recaptchaError && (
            <Card>
              <CardBody>
                <ReCAPTCHA
                  sitekey={this.state.sitekey}
                  onChange={this.onChange}
                  onErrored={() => this.setState({ recaptchaError: true })}
                />
              </CardBody>
            </Card>
          )}
        </div>
      </PageSection>
    );
  }
}
