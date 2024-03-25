/**
 * Portions copyright © 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate
 * affiliate that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy
 * of which is included with this software program.
 */

import * as React from 'react';
import {
  Card,
  CardHead,
  CardHeader,
  CardBody,
  PageSection,
  FormGroup,
  TextInput,
  Title,
  ActionGroup,
  Button,
  Alert,
  AlertActionCloseButton,
  InputGroupText,
  InputGroup,
  Checkbox,
} from '@patternfly/react-core';
import { error, success } from '../Lib/RatApiTypes';
import { getUser, updateUser } from '../Lib/Admin';
import { logger } from '../Lib/Logger';
import { UserContext, UserState, isAdmin, isEditCredential } from '../Lib/User';

// UserAccount.tsx: Page where user properties can be modified
// author: Sam Smucny

interface UserAccountProps {
  userId: number;
  onSuccess?: () => void;
}

interface UserAccountState {
  userId: number;
  email?: string;
  password?: string;
  passwordConf?: string;
  active: boolean;
  messageType?: 'danger' | 'success';
  messageValue: string;
  editCredential: boolean;
}

export class UserAccount extends React.Component<UserAccountProps, UserAccountState> {
  constructor(props: UserAccountProps) {
    super(props);

    this.state = {
      userId: 0,
      active: false,
      messageValue: '',
      email: '',
      password: '',
      passwordConf: '',
      editCredential: false,
    };

    getUser(props.userId).then((res) =>
      res.kind === 'Success'
        ? this.setState({
            email: res.result.email,
            active: res.result.active,
            messageType: undefined,
            messageValue: '',
            editCredential: isEditCredential(),
          } as UserAccountState)
        : this.setState({
            messageType: 'danger',
            messageValue: res.description,
          }),
    );
  }

  private upperLower = (p: string) => /[A-Z]/.test(p) && /[a-z]/.test(p);
  private hasSymbol = (p: string) => /[-!$%^&*()_+|~=`{}\[\]:";'<>@#\)\(\{\}?,.\/\\]/.test(p);

  private validEmail = (e?: string) => {
    if (!this.state.editCredential) return true;
    return !!e && /(\w+)@(\w+).(\w+)/.test(e);
  };

  private validPass = (p?: string) => {
    if (!this.state.editCredential) return true;
    if (!p) return false;
    if (p.length < 8) return false;
    if (!this.upperLower(p)) return false;
    if (!/\d/.test(p)) return false;
    if (!this.hasSymbol(p)) return false;
    return true;
  };

  private validPassConf = (p?: string) => {
    if (!this.state.editCredential) return true;
    return p === this.state.password;
  };

  private updateUser = () => {
    if (this.state.editCredential) {
      if (!this.validEmail(this.state.email)) {
        this.setState({ messageType: 'danger', messageValue: 'Invalid email' });
        return;
      }
      if (!this.validPass(this.state.password)) {
        this.setState({
          messageType: 'danger',
          messageValue:
            'Invalid password:\n Password must contain: minimum 8 characters, a number, upper and lower case letters, and a special character.',
        });
        return;
      }
      if (!this.validPassConf(this.state.passwordConf)) {
        this.setState({ messageType: 'danger', messageValue: 'Passwords must match' });
        return;
      }
    }

    logger.info('Editing user: ', this.state.userId);
    updateUser({
      id: this.props.userId,
      email: this.state.email!,
      active: this.state.active,
      password: this.state.password!,
      editCredential: this.state.editCredential,
    }).then((res) => {
      if (res.kind === 'Success') {
        if (this.props.onSuccess) {
          this.props.onSuccess();
          return;
        }
        this.setState({ messageType: 'success', messageValue: 'User updated' });
      } else {
        this.setState({ messageType: 'danger', messageValue: res.description });
      }
    });
  };

  render = () => {
    return (
      <Card>
        <CardBody>
          {this.state.messageType && (
            <>
              <Alert
                variant={this.state.messageType}
                title={this.state.messageValue || ''}
                action={<AlertActionCloseButton onClose={() => this.setState({ messageType: undefined })} />}
              />
              <br />
            </>
          )}
          {this.state.editCredential ? (
            <FormGroup label="Email" fieldId="form-email">
              <InputGroup>
                <TextInput
                  value={this.state.email}
                  onChange={(x) => this.setState({ email: x })}
                  type="email"
                  id="form-email"
                  name="form-email"
                  isValid={this.validEmail(this.state.email)}
                />
              </InputGroup>
            </FormGroup>
          ) : (
            <FormGroup label="Email" fieldId="form-email">
              <InputGroup>
                <TextInput isReadOnly value={this.state.email} type="email" id="form-email" name="form-email" />
              </InputGroup>
            </FormGroup>
          )}
          {this.state.editCredential && (
            <FormGroup label="Password" fieldId="form-pass">
              <InputGroup>
                <TextInput
                  value={this.state.password}
                  onChange={(x) => this.setState({ password: x })}
                  type="password"
                  id="form-pass"
                  name="form-pass"
                  isValid={this.validPass(this.state.password)}
                />
              </InputGroup>
            </FormGroup>
          )}

          {this.state.editCredential && (
            <FormGroup label="Confirm Password" fieldId="form-pass-c">
              <InputGroup>
                <TextInput
                  value={this.state.passwordConf}
                  onChange={(x) => this.setState({ passwordConf: x })}
                  type="password"
                  id="form-pass-c"
                  name="form-pass-c"
                  isValid={this.validPassConf(this.state.passwordConf)}
                />
              </InputGroup>
            </FormGroup>
          )}

          <br />
          <UserContext.Consumer>
            {(user: UserState) =>
              isAdmin() && (
                <>
                  <Checkbox
                    label="Is Active"
                    aria-label="User is active checkbox"
                    id="user-active-check"
                    name="user-active-check"
                    isChecked={this.state.active}
                    onChange={(c: boolean) => this.setState({ active: c })}
                  />
                  <br />
                </>
              )
            }
          </UserContext.Consumer>
          <Button key="submit" variant="primary" onClick={() => this.updateUser()}>
            Update
          </Button>
        </CardBody>
      </Card>
    );
  };
}

/**
 * wrapper for ap list when it is not embedded in another page
 */
export const UserAccountPage = () => (
  <PageSection id="ap-list-page">
    <Title size="3xl">Edit User</Title>
    <UserContext.Consumer>
      {(u: UserState) => u.data.loggedIn && <UserAccount userId={u.data.userId} />}
    </UserContext.Consumer>
  </PageSection>
);
