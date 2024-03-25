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
import { logger } from '../Lib/Logger';
import ReCAPTCHA from 'react-google-recaptcha';
import { setAboutAfc } from '../Lib/RatApi';
import { PlusCircleIcon } from '@patternfly/react-icons';

export class About extends React.Component {
  constructor(props: Readonly<{ content: RatResponse<string>; sitekey: string }>) {
    super();
    this.state = { content: props.content.result, name: '', email: '', org: '', token: '', sitekey: props.sitekey };
    logger.info('ABOUT state: ' + this.state.content);
  }

  onChange = (value) => {
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
      //
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
    const nameChange = (s?: string) => this.setState({ name: s });
    const emailChange = (s?: string) => this.setState({ email: s });
    const orgChange = (s?: string) => this.setState({ org: s });

    return (
      <PageSection>
        <div>
          <Title size={'lg'}>Request Access to the AFC Website</Title>
          <Card>
            {' '}
            <CardBody dangerouslySetInnerHTML={{ __html: this.state.content }} />
          </Card>
          <Card>
            <FormGroup label="Full Name" isRequired={true} fieldId="user-full-name">
              <TextInput
                type="text"
                id="user-full-name"
                name="user-full-name"
                isValid={true}
                value={this.state.name}
                onChange={nameChange}
              />
            </FormGroup>
          </Card>
          <Card>
            <FormGroup label="Email" isRequired={true} fieldId="user-email">
              <TextInput
                type="text"
                id="user-email"
                name="user-email"
                isValid={true}
                value={this.state.email}
                onChange={emailChange}
              />
            </FormGroup>
          </Card>
          <Card>
            <FormGroup label="Organization" isRequired={true} fieldId="user-org">
              <TextInput
                type="text"
                id="user-org"
                name="user-org"
                isValid={true}
                value={this.state.org}
                onChange={orgChange}
              />
            </FormGroup>
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
          <br />

          <Button variant="primary" icon={<PlusCircleIcon />} onClick={() => this.submit()}>
            Submit
          </Button>

          <Card>{this.state.sitekey && <ReCAPTCHA sitekey={this.state.sitekey} onChange={this.onChange} />}</Card>
        </div>
      </PageSection>
    );
  }
}
